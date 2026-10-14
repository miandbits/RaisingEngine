// Derive presentation signals from one committed game transition.
// Signals depend only on (before, after, command, error) so every host emits
// exactly the same list once per state revision, independent of frame rate.
#include "cutout/signals.hpp"

namespace cutout {
namespace {
const Json& field(const Json& obj, const char* key) {
  static const Json null = nullptr;
  return obj.is_object() && obj.contains(key) ? obj.at(key) : null;
}
std::string text(const Json& obj, const char* key) {
  const auto& v = field(obj, key);
  return v.is_string() ? v.get<std::string>() : "";
}
}  // namespace

std::vector<Signal> deriveSignals(const Json& before, const Json& after, const Json& command, const std::string& error) {
  std::vector<Signal> out;
  const auto& sb = field(before, "state");
  const auto& sa = field(after, "state");
  const std::string type = text(command, "type");

  if (type == "executeAction") {
    const std::string id = text(command, "actionId");
    if (error.empty()) {
      out.push_back({"actionStarted", {{"actionId", id}}});
      out.push_back({"actionResult", {{"actionId", id}, {"outcome", "success"}}});
    } else out.push_back({"actionResult", {{"actionId", id}, {"outcome", "failure"}}});
  }
  if (!error.empty()) return out;

  // Activity results are committed in state; a new (activityId,time) pair is a new result.
  const auto& rb = field(sb, "lastActivityResult");
  const auto& ra = field(sa, "lastActivityResult");
  if (ra.is_object() && (!rb.is_object() || field(rb, "activityId") != field(ra, "activityId") || field(rb, "time") != field(ra, "time"))) {
    const std::string id = text(ra, "activityId");
    const auto& success = field(ra, "success");
    const std::string outcome = success.is_object() ? (field(success, "passed") == true ? "success" : "failure") : "done";
    out.push_back({"activityStarted", {{"activityId", id}}});
    out.push_back({"activityResult", {{"activityId", id}, {"outcome", outcome}}});
  }

  // Scene nodes change portraits: treat a new portrait id for a character as an expression change.
  std::map<std::string, std::string> previous;
  const auto& pb = field(field(sb, "presentation"), "portraits");
  if (pb.is_array()) for (const auto& p : pb) previous[text(p, "characterId")] = text(p, "portraitId");
  const auto& pa = field(field(sa, "presentation"), "portraits");
  if (pa.is_array())
    for (const auto& p : pa) {
      const std::string character = text(p, "characterId"), portrait = text(p, "portraitId");
      auto it = previous.find(character);
      if (it == previous.end() || it->second != portrait)
        out.push_back({"portraitChanged", {{"characterId", character}, {"portraitId", portrait}, {"position", text(p, "position")}}});
    }

  const auto& nb = field(field(before, "narrative"), "node");
  const auto& na = field(field(after, "narrative"), "node");
  const bool lineBefore = text(nb, "type") == "dialogue", lineAfter = text(na, "type") == "dialogue";
  if (lineAfter && (!lineBefore || field(field(before, "narrative"), "cursor") != field(field(after, "narrative"), "cursor")))
    out.push_back({"dialogueLine", {{"speakerId", text(na, "speakerId")}, {"nodeId", text(na, "id")}}});
  if (lineBefore && !lineAfter) out.push_back({"dialogueEnded", {{"speakerId", text(nb, "speakerId")}}});

  if (field(sb, "phase") != field(sa, "phase") && field(sa, "phase").is_string())
    out.push_back({"phaseChanged", {{"phase", text(sa, "phase")}}});
  return out;
}

Json signalJson(const Signal& s) {
  Json params = Json::object();
  for (const auto& [k, v] : s.params) params[k] = v;
  return {{"name", s.name}, {"params", params}};
}
}  // namespace cutout
