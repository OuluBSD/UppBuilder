#include "RuntimeDeps.h"

namespace UppBuilder {

static std::vector<unsigned char> ReadBinaryBytes(const std::string& path) {
    std::ifstream is(path, std::ios::binary);
    if (!is) return {};
    is.seekg(0, std::ios::end);
    size_t sz = is.tellg();
    is.seekg(0, std::ios::beg);
    std::vector<unsigned char> buf(sz);
    is.read(reinterpret_cast<char*>(buf.data()), sz);
    return buf;
}

static uint16_t ReadU16(const std::vector<unsigned char>& buf, size_t off) {
    if (off + 2 > buf.size()) return 0;
    return (uint16_t)buf[off] | ((uint16_t)buf[off + 1] << 8);
}

static uint32_t ReadU32(const std::vector<unsigned char>& buf, size_t off) {
    if (off + 4 > buf.size()) return 0;
    return (uint32_t)buf[off] | ((uint32_t)buf[off + 1] << 8) |
           ((uint32_t)buf[off + 2] << 16) | ((uint32_t)buf[off + 3] << 24);
}

static uint64_t ReadU64(const std::vector<unsigned char>& buf, size_t off) {
    if (off + 8 > buf.size()) return 0;
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= ((uint64_t)buf[off + i]) << (8 * i);
    return v;
}

std::vector<std::string> RuntimeDepsHarvester::ExtractPeDependencies(const std::string& path) {
    std::vector<std::string> result;
    auto buf = ReadBinaryBytes(path);
    if (buf.size() < 0x40) return result;
    if (buf[0] != 'M' || buf[1] != 'Z') return result;

    uint32_t pe_off = ReadU32(buf, 0x3C);
    if (pe_off + 4 > buf.size()) return result;
    if (buf[pe_off] != 'P' || buf[pe_off + 1] != 'E' || buf[pe_off + 2] != 0 || buf[pe_off + 3] != 0) return result;

    uint16_t num_sections = ReadU16(buf, pe_off + 6);
    uint16_t opt_hdr_sz = ReadU16(buf, pe_off + 20);
    size_t opt_hdr_off = pe_off + 24;
    if (opt_hdr_off + opt_hdr_sz > buf.size()) return result;

    uint16_t magic = ReadU16(buf, opt_hdr_off);
    bool is_pe32plus = (magic == 0x20B);

    size_t import_dir_entry = is_pe32plus ? (opt_hdr_off + 120) : (opt_hdr_off + 104);
    uint32_t import_rva = ReadU32(buf, import_dir_entry);
    if (!import_rva) return result;

    size_t sect_hdr_off = opt_hdr_off + opt_hdr_sz;
    struct Section { uint32_t va, sz, raw_off; };
    std::vector<Section> sections;
    for (int i = 0; i < num_sections; ++i) {
        size_t o = sect_hdr_off + i * 40;
        if (o + 40 > buf.size()) break;
        sections.push_back({ReadU32(buf, o + 12), ReadU32(buf, o + 8), ReadU32(buf, o + 20)});
    }

    auto rva_to_off = [&](uint32_t rva) -> size_t {
        for (const auto& s : sections) {
            if (rva >= s.va && rva < s.va + s.sz) {
                return (size_t)s.raw_off + (rva - s.va);
            }
        }
        return 0;
    };

    size_t import_off = rva_to_off(import_rva);
    if (!import_off || import_off + 20 > buf.size()) return result;

    for (size_t o = import_off; o + 20 <= buf.size(); o += 20) {
        uint32_t name_rva = ReadU32(buf, o + 12);
        if (!name_rva) break;
        size_t name_off = rva_to_off(name_rva);
        if (name_off && name_off < buf.size()) {
            std::string name;
            while (name_off < buf.size() && buf[name_off] != 0) {
                name += (char)buf[name_off++];
            }
            if (!name.empty()) result.push_back(name);
        }
    }
    return result;
}

std::vector<std::string> RuntimeDepsHarvester::ExtractElfDependencies(const std::string& path) {
    std::vector<std::string> result;
    auto buf = ReadBinaryBytes(path);
    if (buf.size() < 0x40) return result;
    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F') return result;
    if (buf[4] != 2) return result; // 64-bit ELF

    uint64_t shoff = ReadU64(buf, 0x28);
    uint16_t shentsize = ReadU16(buf, 0x3A);
    uint16_t shnum = ReadU16(buf, 0x3C);
    if (shentsize < 64 || shoff > buf.size()) return result;

    struct SH { uint32_t type; uint64_t offset, size; uint32_t link; };
    std::vector<SH> shs;
    for (int i = 0; i < shnum; ++i) {
        size_t o = shoff + (size_t)i * shentsize;
        if (o + 64 > buf.size()) break;
        shs.push_back({ReadU32(buf, o + 4), ReadU64(buf, o + 0x18), ReadU64(buf, o + 0x20), ReadU32(buf, o + 0x28)});
    }

    for (const auto& s : shs) {
        if (s.type != 6) continue; // SHT_DYNAMIC
        uint64_t dynstr_off = (s.link < shs.size()) ? shs[s.link].offset : 0;
        if (!dynstr_off) break;
        uint64_t dyn_end = s.offset + s.size;
        for (uint64_t off = s.offset; off + 16 <= dyn_end && off + 16 <= buf.size(); off += 16) {
            int64_t tag = (int64_t)ReadU64(buf, off);
            uint64_t val = ReadU64(buf, off + 8);
            if (tag == 0) break; // DT_NULL
            if (tag == 1) { // DT_NEEDED
                size_t so = dynstr_off + val;
                if (so < buf.size()) {
                    std::string name;
                    while (so < buf.size() && buf[so] != 0) name += (char)buf[so++];
                    if (!name.empty()) result.push_back(name);
                }
            }
        }
        break;
    }
    return result;
}

std::vector<std::string> RuntimeDepsHarvester::ExtractMachODependencies(const std::string& path) {
    std::vector<std::string> result;
    // Fallback: parse otool output if on macOS
#ifdef __APPLE__
    std::string out = CaptureCommand("otool -L \"" + path + "\"");
    std::stringstream ss(out);
    std::string line;
    while (std::getline(ss, line)) {
        line = Trim(line);
        if (line.empty() || EndsWith(line, ":")) continue;
        size_t sp = line.find(' ');
        if (sp != std::string::npos) {
            result.push_back(line.substr(0, sp));
        }
    }
#endif
    return result;
}

std::vector<std::string> RuntimeDepsHarvester::ExtractDependencies(const std::string& binary_path) {
#ifdef _WIN32
    return ExtractPeDependencies(binary_path);
#elif defined(__APPLE__)
    return ExtractMachODependencies(binary_path);
#else
    return ExtractElfDependencies(binary_path);
#endif
}

bool RuntimeDepsHarvester::IsSystemDependency(const std::string& library_path) {
    std::string p = NormalizePath(library_path);
#ifdef _WIN32
    std::string lower = ToLower(GetFileName(p));
    if (StartsWith(lower, "api-ms-win-") || StartsWith(lower, "ext-ms-")) return true;
    std::string sys = ToLower(GetEnvValue("SystemRoot"));
    if (!sys.empty() && StartsWith(ToLower(p), sys)) return true;
#else
    static const char* kSysPrefixes[] = {
        "/lib/", "/lib64/", "/usr/lib/", "/usr/lib64/", "/System/", "/usr/lib/system/"
    };
    for (const char* pref : kSysPrefixes) {
        if (StartsWith(p, pref)) return true;
    }
    std::string fn = GetFileName(p);
    if (StartsWith(fn, "libc.") || StartsWith(fn, "libm.") || StartsWith(fn, "libpthread.") ||
        StartsWith(fn, "libdl.") || StartsWith(fn, "librt.")) {
        return true;
    }
#endif
    return false;
}

void RuntimeDepsHarvester::CopyDependenciesRecursive(const std::string& binary_path,
                                                     const std::string& target_dir,
                                                     const std::vector<std::string>& search_paths,
                                                     std::set<std::string>& out_copied) {
    std::vector<std::string> deps = ExtractDependencies(binary_path);
    for (const auto& dep : deps) {
        if (IsSystemDependency(dep)) continue;
        std::string dep_fn = GetFileName(dep);
        if (out_copied.count(dep_fn)) continue;

        // Search for file in search_paths
        std::string found_path;
        for (const auto& sp : search_paths) {
            std::string cand = JoinPath(sp, dep_fn);
            if (FileExists(cand)) {
                found_path = cand;
                break;
            }
        }

        if (!found_path.empty()) {
            std::string dest = JoinPath(target_dir, dep_fn);
            if (GetAbsolutePath(found_path) != GetAbsolutePath(dest)) {
                if (CopyFile(found_path, dest)) {
                    out_copied.insert(dep_fn);
                    CopyDependenciesRecursive(dest, target_dir, search_paths, out_copied);
                }
            }
        }
    }
}

} // namespace UppBuilder
