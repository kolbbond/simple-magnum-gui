// taken from Van Nugterens project-rat
#pragma once

#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>

#include "log.hh"

// filename macro for shortening the path (handles both POSIX and Windows separators)
#define SMG_FILENAME (std::strrchr(__FILE__, '/') ? std::strrchr(__FILE__, '/') + 1 : __FILE__)

namespace smg {

// runtime_error carrying filename/function/line. The decorated message is baked
// into the std::runtime_error base (no std::string member) so copying the
// in-flight exception is nothrow, as exception types must be.
class smg_error: public std::runtime_error {
public:
    smg_error(const std::string& arg, const char* file, const char* function, int line)
        : std::runtime_error(format(arg, file, function, line)) {}

private:
    static std::string format(const std::string& arg, const char* file, const char* function, int line) {
        std::ostringstream o;
        o << SMG_KRED << SMG_KBLD << "error: " << SMG_KNRM << file << ":" << function << ":" << SMG_KGRN << SMG_KBLD << line << SMG_KNRM
          << ": " << '\n' << " " SMG_KYEL << arg << SMG_KNRM;
        return o.str();
    }
};

} // namespace smg

// throw an smg_error tagged with the call site; no trailing ';' so it composes in if/else
#define smg_throw_line(arg) throw smg::smg_error(arg, SMG_FILENAME, __FUNCTION__, __LINE__)
