// Clip templates: generate ordinary, editable clips (never procedural runtime effects).
#pragma once
#include "editing.hpp"

namespace cutout {
/** breathing params: {bone, property: "scaleY"|"y", amplitude, periodMs}. */
Json breathingClip(const Json& rig, const std::string& clipId, const Json& params);
/** blink params: {slot, open, half?, closed, intervalMs}. */
Json blinkClip(const Json& rig, const std::string& clipId, const Json& params);
/** Dispatch by kind ("breathing" | "blink"). Throws EditError on bad params. */
Json templateClip(const std::string& kind, const Json& rig, const Json& params, const std::string& clipId);
}  // namespace cutout
