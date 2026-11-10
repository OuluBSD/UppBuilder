#include "UseModel.h"

namespace UppBuilder {

const std::vector<UsePolicy>& UseModelEngine::GetBuiltinPolicies() {
    static const std::vector<UsePolicy> policies = []() {
        std::vector<UsePolicy> list;

        {
            UsePolicy p;
            p.name = "sqlite";
            p.description = "Enable SQLite embedded relational database engine";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "SQLITE";
            m.scope = UseScope::ACCEPTED;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "st";
            p.description = "Build single-threaded runtime variant";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "ST";
            m.scope = UseScope::GLOBAL;
            m.thread_model = "st";
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "gtk";
            p.description = "Enable GTK UI integration backend";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "GTK";
            m.scope = UseScope::ACCEPTED;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "virtualgui";
            p.description = "Enable headless VirtualGui framebuffer transport";
            p.default_on = false;
            {
                UsePolicyMap m;
                m.upp_flag = "GUI";
                m.scope = UseScope::GLOBAL;
                p.maps_to.push_back(m);
            }
            {
                UsePolicyMap m;
                m.upp_flag = "SDL2GL";
                m.scope = UseScope::GLOBAL;
                p.maps_to.push_back(m);
            }
            {
                UsePolicyMap m;
                m.upp_flag = "VIRTUALGUI";
                m.scope = UseScope::ACCEPTED;
                p.maps_to.push_back(m);
            }
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "ssl";
            p.description = "Enable OpenSSL / system cryptographic transport";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "SSL";
            m.scope = UseScope::ACCEPTED;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "opengl";
            p.description = "Enable hardware accelerated OpenGL rendering";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "OPENGL";
            m.scope = UseScope::ACCEPTED;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "shared";
            p.description = "Build dynamic shared libraries (.so / .dll / .dylib)";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "SO";
            m.scope = UseScope::GLOBAL;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "blitz";
            p.description = "Enable BLITZ single-compilation-unit speedup";
            p.default_on = true;
            UsePolicyMap m;
            m.upp_flag = "BLITZ";
            m.scope = UseScope::GLOBAL;
            p.maps_to.push_back(m);
            list.push_back(p);
        }
        {
            UsePolicy p;
            p.name = "turtle";
            p.description = "Enable Turtle remote HTML5 web terminal UI";
            p.default_on = false;
            UsePolicyMap m;
            m.upp_flag = "TURTLE";
            m.scope = UseScope::ACCEPTED;
            p.maps_to.push_back(m);
            list.push_back(p);
        }

        return list;
    }();
    return policies;
}

const UsePolicy* UseModelEngine::FindPolicy(const std::string& name) {
    std::string low = ToLower(Trim(name));
    if (low.empty()) return nullptr;
    const auto& policies = GetBuiltinPolicies();
    for (const auto& p : policies) {
        if (ToLower(p.name) == low) {
            return &p;
        }
    }
    return nullptr;
}

UseScope UseModelEngine::ScopeFromText(const std::string& text) {
    std::string low = ToLower(Trim(text));
    if (low == "accepted" || low == "accept") return UseScope::ACCEPTED;
    if (low == "main_only" || low == "main") return UseScope::MAIN_ONLY;
    return UseScope::GLOBAL;
}

std::string UseModelEngine::ScopeToText(UseScope scope) {
    switch (scope) {
        case UseScope::ACCEPTED:  return "accepted";
        case UseScope::MAIN_ONLY: return "main_only";
        default:                  return "global";
    }
}

std::vector<std::string> UseModelEngine::ParseUseString(const std::string& use_str) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : use_str) {
        if (std::isspace(static_cast<unsigned char>(c)) || c == ',') {
            if (!cur.empty()) {
                tokens.push_back(Trim(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        tokens.push_back(Trim(cur));
    }
    return tokens;
}

void UseModelEngine::TokenizeUseArgs(const std::vector<std::string>& args,
                                     std::set<std::string>& selected,
                                     std::set<std::string>& disabled) {
    for (const auto& raw : args) {
        std::string token = Trim(raw);
        if (token.empty()) continue;
        if (token[0] == '+') {
            std::string name = ToLower(Trim(token.substr(1)));
            if (!name.empty()) {
                selected.insert(name);
                disabled.erase(name);
            }
        } else if (token[0] == '-') {
            std::string name = ToLower(Trim(token.substr(1)));
            if (!name.empty()) {
                disabled.insert(name);
                selected.erase(name);
            }
        } else {
            std::string name = ToLower(token);
            selected.insert(name);
            disabled.erase(name);
        }
    }
}

UseModel UseModelEngine::BuildUseModel(const UppPackage* pkg,
                                       const std::vector<std::string>& use_args,
                                       const std::vector<std::string>& forced_flags,
                                       const std::vector<std::string>& masked_flags) {
    UseModel model;
    model.requested = use_args;

    std::set<std::string> declared_set;
    if (pkg) {
        for (const auto& f : pkg->acceptflags) {
            declared_set.insert(ToLower(f));
        }
    }
    for (const auto& p : GetBuiltinPolicies()) {
        declared_set.insert(ToLower(p.name));
        if (p.default_on) {
            model.defaults.push_back(ToLower(p.name));
        }
    }

    // Tokenize requested
    std::set<std::string> selected_set;
    std::set<std::string> disabled_set;
    TokenizeUseArgs(use_args, selected_set, disabled_set);

    for (const auto& s : selected_set) declared_set.insert(s);
    for (const auto& d : disabled_set) declared_set.insert(d);

    for (const auto& f : forced_flags) {
        std::string name = ToLower(Trim(f));
        if (!name.empty()) {
            model.forced.push_back(name);
            declared_set.insert(name);
        }
    }
    for (const auto& m : masked_flags) {
        std::string name = ToLower(Trim(m));
        if (!name.empty()) {
            model.masked.push_back(name);
            declared_set.insert(name);
        }
    }

    for (const auto& d : declared_set) {
        model.declared.push_back(d);
    }
    std::sort(model.declared.begin(), model.declared.end());

    // Apply forced & masked to selected & disabled
    for (const auto& f : model.forced) {
        selected_set.insert(f);
        if (disabled_set.count(f)) {
            model.conflicts.push_back(f);
        }
    }
    for (const auto& m : model.masked) {
        disabled_set.insert(m);
        if (selected_set.count(m)) {
            model.conflicts.push_back(m);
            selected_set.erase(m);
        }
    }

    for (const auto& s : selected_set) model.selected.push_back(s);
    for (const auto& d : disabled_set) model.disabled.push_back(d);
    std::sort(model.selected.begin(), model.selected.end());
    std::sort(model.disabled.begin(), model.disabled.end());
    std::sort(model.conflicts.begin(), model.conflicts.end());

    // Compute transitions
    for (const auto& s : model.selected) {
        bool is_forced = (std::find(model.forced.begin(), model.forced.end(), s) != model.forced.end());
        model.transitions.push_back({s, "+", is_forced ? "forced" : "selected"});
    }
    for (const auto& def : model.defaults) {
        if (!selected_set.count(def) && !disabled_set.count(def)) {
            model.transitions.push_back({def, "+", "default"});
        }
    }
    for (const auto& d : model.disabled) {
        bool is_masked = (std::find(model.masked.begin(), model.masked.end(), d) != model.masked.end());
        model.transitions.push_back({d, "-", is_masked ? "masked" : "disabled"});
    }

    // Compute effective flags
    std::set<std::string> eff_set;
    for (const auto& s : selected_set) {
        if (!disabled_set.count(s)) {
            eff_set.insert(s);
        }
    }
    for (const auto& def : model.defaults) {
        if (!disabled_set.count(def)) {
            eff_set.insert(def);
        }
    }
    for (const auto& f : model.forced) {
        if (!disabled_set.count(f) || std::find(model.masked.begin(), model.masked.end(), f) == model.masked.end()) {
            eff_set.insert(f);
        }
    }

    for (const auto& e : eff_set) {
        model.effective.push_back(e);
    }
    std::sort(model.effective.begin(), model.effective.end());

    return model;
}

UppProjection UseModelEngine::ProjectUpp(const UseModel& use, const UppPackage* pkg) {
    UppProjection proj;
    std::set<std::string> seen;

    std::set<std::string> pkg_accepts;
    if (pkg) {
        for (const auto& a : pkg->acceptflags) {
            pkg_accepts.insert(ToUpper(Trim(a)));
        }
    }

    for (const auto& eff : use.effective) {
        const UsePolicy* policy = FindPolicy(eff);
        if (policy) {
            for (const auto& m : policy->maps_to) {
                std::string flag = ToUpper(Trim(m.upp_flag));
                if (flag.empty() || seen.count(flag)) continue;
                seen.insert(flag);

                UppFlag uf;
                uf.name = flag;
                uf.scope = m.scope;
                uf.reason = "policy:" + policy->name;
                proj.flags.push_back(uf);

                if (m.scope == UseScope::GLOBAL) {
                    proj.global.push_back(flag);
                } else if (m.scope == UseScope::ACCEPTED) {
                    if (!pkg || pkg_accepts.empty() || pkg_accepts.count(flag)) {
                        proj.accepted.push_back(flag);
                    }
                } else if (m.scope == UseScope::MAIN_ONLY) {
                    proj.main_only.push_back(flag);
                }
            }
        } else {
            // Implicit direct flag mapping
            std::string flag = ToUpper(Trim(eff));
            if (flag.empty() || seen.count(flag)) continue;
            seen.insert(flag);

            UppFlag uf;
            uf.name = flag;
            uf.reason = "direct";

            if (pkg && !pkg_accepts.empty() && pkg_accepts.count(flag)) {
                uf.scope = UseScope::ACCEPTED;
                proj.accepted.push_back(flag);
            } else {
                uf.scope = UseScope::GLOBAL;
                proj.global.push_back(flag);
            }
            proj.flags.push_back(uf);
        }
    }

    std::sort(proj.global.begin(), proj.global.end());
    std::sort(proj.accepted.begin(), proj.accepted.end());
    std::sort(proj.main_only.begin(), proj.main_only.end());

    return proj;
}

std::string UseModelEngine::FormatUseModel(const UseModel& use) {
    std::ostringstream ss;
    ss << "USE Flag Model:\n";
    ss << "  Requested:   ";
    if (use.requested.empty()) ss << "(none)";
    else ss << JoinStrings(use.requested, " ");
    ss << "\n";

    ss << "  Effective:   ";
    if (use.effective.empty()) ss << "(none)";
    else {
        std::vector<std::string> eff_display;
        for (const auto& e : use.effective) {
            eff_display.push_back("+" + e);
        }
        for (const auto& d : use.disabled) {
            eff_display.push_back("-" + d);
        }
        ss << JoinStrings(eff_display, " ");
    }
    ss << "\n";

    if (!use.conflicts.empty()) {
        ss << "  Conflicts:   " << JoinStrings(use.conflicts, ", ") << "\n";
    }

    ss << "  Transitions: ";
    if (use.transitions.empty()) ss << "(none)\n";
    else {
        ss << "\n";
        for (const auto& tr : use.transitions) {
            ss << "    " << tr.marker << tr.flag << " (" << tr.reason << ")\n";
        }
    }
    return ss.str();
}

std::string UseModelEngine::FormatProjection(const UppProjection& proj) {
    std::ostringstream ss;
    ss << "U++ Compile Flag Projections:\n";
    ss << "  Global Flags:    " << (proj.global.empty() ? "(none)" : JoinStrings(proj.global, " ")) << "\n";
    ss << "  Accepted Flags:  " << (proj.accepted.empty() ? "(none)" : JoinStrings(proj.accepted, " ")) << "\n";
    ss << "  Main-Only Flags: " << (proj.main_only.empty() ? "(none)" : JoinStrings(proj.main_only, " ")) << "\n";
    return ss.str();
}

std::string UseModelEngine::ExplainUse(const std::string& flag_name) {
    std::ostringstream ss;
    const UsePolicy* p = FindPolicy(flag_name);
    if (!p) {
        ss << "USE Flag: " << flag_name << " (direct/unmapped)\n";
        ss << "  Description: Direct compiler flag projected as " << ToUpper(flag_name) << "\n";
        ss << "  Default:     disabled\n";
        ss << "  Scope:       accepted (if declared in package) or global\n";
        return ss.str();
    }

    ss << "USE Flag: " << p->name << "\n";
    ss << "  Description: " << p->description << "\n";
    ss << "  Default:     " << (p->default_on ? "enabled (+)" : "disabled (-)") << "\n";
    ss << "  Mappings:\n";
    for (const auto& m : p->maps_to) {
        ss << "    -> " << m.upp_flag << " (scope: " << ScopeToText(m.scope);
        if (!m.thread_model.empty()) ss << ", thread_model: " << m.thread_model;
        ss << ")\n";
    }
    return ss.str();
}

} // namespace UppBuilder
