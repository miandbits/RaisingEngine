// Game -> presentation signal vocabulary (v1). See docs/contract.md "Signals".
#pragma once
#include "rig.hpp"

namespace cutout {
struct Signal { std::string name; std::map<std::string, std::string> params; };
/**
 * before/after: {"state": <core state>, "narrative": <core view.narrative or null>}.
 * command: the submitted command JSON. error: empty when the command committed.
 */
std::vector<Signal> deriveSignals(const Json& before, const Json& after, const Json& command, const std::string& error);
Json signalJson(const Signal& s);
}  // namespace cutout
