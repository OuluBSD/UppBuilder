#include <iostream>

int main() {
#if flagFEATURE_A
    std::cout << "PolicyTestPkg: FEATURE_A is enabled\n";
#endif
#if flagFEATURE_B
    std::cout << "PolicyTestPkg: FEATURE_B is enabled\n";
#endif
#if flagSQLITE
    std::cout << "PolicyTestPkg: SQLITE is enabled\n";
#endif
    return 0;
}
