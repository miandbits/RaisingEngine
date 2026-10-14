#pragma once
#include <string>
namespace raising {
// Pure portable core: no filesystem, windowing, clock or platform APIs.
// JSON is the stable boundary; each response owns its serialized memory.
std::string process(const std::string &request);
} // namespace raising
