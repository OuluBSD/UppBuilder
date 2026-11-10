#include <iostream>

#ifdef flagDECLARED_FLAG
// Declared flag in acceptflags
#endif

#if defined(flagMISSING_FLAG)
// Unaccepted flag missing in acceptflags
#endif

int main() {
    return 0;
}
