#pragma once

#include <string>

namespace Util {
    bool isAbsolutePath(const std::string& path);
    std::string getExecutableDirectory();

    namespace Date {
        int64_t now();
    }
}
