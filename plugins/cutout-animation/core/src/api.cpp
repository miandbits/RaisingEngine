// JSON command host shared by the Windows runtime DLL, WebAssembly and tests.
#include "cutout/api.h"
#include "cutout/host.hpp"

int cutout_api_version(void) { return CUTOUT_API_VERSION; }

const char* cutout_request(const char* request_json) {
  static thread_local cutout::Host host;
  static thread_local std::string reply;
  reply = host.request(request_json ? request_json : "").dump();
  return reply.c_str();
}
