/*
 * Cutout animation runtime C ABI (api version 1).
 * One JSON request/response entry point keeps the native DLL and the WebAssembly
 * module identical. The returned pointer stays valid until the next call on the
 * same thread. Hosts never pass ownership across this boundary.
 */
#ifndef CUTOUT_API_H
#define CUTOUT_API_H
#ifdef __cplusplus
extern "C" {
#endif
#if defined(_WIN32) && defined(CUTOUT_BUILD_DLL)
#define CUTOUT_EXPORT __declspec(dllexport)
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#define CUTOUT_EXPORT EMSCRIPTEN_KEEPALIVE
#else
#define CUTOUT_EXPORT
#endif
#define CUTOUT_API_VERSION 1
CUTOUT_EXPORT int cutout_api_version(void);
CUTOUT_EXPORT const char* cutout_request(const char* request_json);
#ifdef __cplusplus
}
#endif
#endif
