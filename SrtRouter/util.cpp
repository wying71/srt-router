#include "util.h"
#include <chrono>
#include <boost/filesystem.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

namespace Util {

bool isAbsolutePath(const std::string& str) {
    return boost::filesystem::path(str).is_absolute();
}

std::string getExecutableDirectory() {
    char szTemp[4096] = { 0 };

#ifdef _WIN32
    GetModuleFileNameA(NULL, szTemp, sizeof(szTemp));
    char* szEnd = strrchr(szTemp, '\\');
    if (szEnd != NULL) {
        *szEnd = '\0';
    }
#else
    int rslt = readlink("/proc/self/exe", szTemp, sizeof(szTemp) - 1);
    if (rslt < 0 || rslt >= (int)(sizeof(szTemp) - 1)) {
        return ".";
    }
    szTemp[rslt] = '\0';

    // Find the last '/' to get directory
    for (int i = rslt; i >= 0; i--) {
        if (szTemp[i] == '/') {
            szTemp[i] = '\0';
            break;
        }
    }
#endif

    return std::string(szTemp);
}

namespace Date {
    int64_t now() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
                ).count();
    }
}

} // namespace util
