#include <node_api.h>
#include <raising/runtime.hpp>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace {
// Resolve the stable C ABI from the loading executable on Windows. Electron
// exports it from electron.exe, Node from node.exe. No node.dll import or V8 /
// Node C++ ABI is involved; this is the same host resolution as a delay hook.
struct Api {
#ifdef _WIN32
#define API(name) decltype(&name) name = reinterpret_cast<decltype(&::name)>(GetProcAddress(GetModuleHandleW(nullptr), #name));
#else
#define API(name) decltype(&name) name = &::name;
#endif
  API(napi_get_cb_info)
  API(napi_typeof)
  API(napi_get_value_string_utf8)
  API(napi_create_string_utf8)
  API(napi_create_function)
  API(napi_set_named_property)
  API(napi_throw_error)
  API(napi_throw_type_error)
#undef API
  bool valid() const {
    return napi_get_cb_info && napi_typeof && napi_get_value_string_utf8 &&
           napi_create_string_utf8 && napi_create_function && napi_set_named_property &&
           napi_throw_error && napi_throw_type_error;
  }
};
const Api& api() { static const Api value; return value; }

napi_value request(napi_env env, napi_callback_info info) {
  const auto& a = api();
  try {
    size_t count = 1;
    napi_value input;
    napi_valuetype type;
    if (a.napi_get_cb_info(env, info, &count, &input, nullptr, nullptr) != napi_ok ||
        count != 1 || a.napi_typeof(env, input, &type) != napi_ok || type != napi_string) {
      a.napi_throw_type_error(env, nullptr, "Native request must be a JSON string.");
      return nullptr;
    }
    size_t length;
    if (a.napi_get_value_string_utf8(env, input, nullptr, 0, &length) != napi_ok)
      throw std::runtime_error("Cannot read native request.");
    if (length > 64 * 1024 * 1024)
      throw std::runtime_error("Native request exceeds 64 MiB.");
    std::string text(length + 1, '\0');
    if (a.napi_get_value_string_utf8(env, input, text.data(), text.size(), &length) != napi_ok)
      throw std::runtime_error("Cannot decode native request.");
    text.resize(length);
    const auto response = raising::process(text);
    napi_value output;
    if (a.napi_create_string_utf8(env, response.data(), response.size(), &output) != napi_ok)
      throw std::runtime_error("Cannot encode native response.");
    return output;
  } catch (const std::exception& error) {
    a.napi_throw_error(env, nullptr, error.what());
    return nullptr;
  } catch (...) {
    a.napi_throw_error(env, nullptr, "Unknown native addon error.");
    return nullptr;
  }
}
} // namespace

NAPI_MODULE_INIT() {
  const auto& a = api();
  if (!a.valid()) return nullptr;
  napi_value function;
  if (a.napi_create_function(env, "request", NAPI_AUTO_LENGTH, request, nullptr, &function) != napi_ok ||
      a.napi_set_named_property(env, exports, "request", function) != napi_ok) return nullptr;
  return exports;
}
