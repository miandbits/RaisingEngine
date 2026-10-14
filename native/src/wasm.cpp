#include "raising/runtime.hpp"
#include <string>

extern "C" {
// Result remains valid until the next call; callers copy the UTF-8 string.
// No filesystem or JavaScript rule evaluation is used by this export.
const char *raising_process(const char *request) {
  static std::string result;
  result = raising::process(request ? request : "null");
  return result.c_str();
}
}
