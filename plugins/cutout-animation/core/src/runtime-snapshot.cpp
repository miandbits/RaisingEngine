#include "cutout/runtime.hpp"
#include <cmath>
#include <stdexcept>

namespace cutout {
namespace {
double number(const Json& j, const char* key, double minimum = -1e12, double maximum = 1e12) {
  const auto& v = j.at(key);
  if (!v.is_number()) throw std::runtime_error(std::string("Invalid animation checkpoint: ") + key);
  const double n = v.get<double>();
  if (!std::isfinite(n) || n < minimum || n > maximum)
    throw std::runtime_error(std::string("Invalid animation checkpoint: ") + key);
  return n;
}
uint64_t counter(const Json& j, const char* key) {
  const auto& v = j.at(key);
  if (!v.is_number_integer() || v.get<double>() < 0 || v.get<double>() > 1e12)
    throw std::runtime_error(std::string("Invalid animation checkpoint: ") + key);
  return v.get<uint64_t>();
}
}

Json Instance::savePresentationState() const {
  const auto entry = [this](const Entry& e) {
    return Json{{"clip", rig_->clips[e.clip].id}, {"timeMs", e.timeMs}, {"loop", e.loop},
      {"speed", e.speed}, {"then", e.then == Then::Hold ? "hold" : e.then == Then::Stop ? "stop" : "default"},
      {"binding", e.binding}, {"serial", e.serial}, {"loops", e.loops}, {"ended", e.ended}};
  };
  Json tracks = Json::array(), bodies = Json::array();
  for (const auto& [track, state] : tracks_) {
    tracks.push_back({{"track", track}, {"current", state.current ? entry(*state.current) : Json()},
      {"from", state.from ? entry(*state.from) : Json()}, {"mixElapsed", state.mixElapsed},
      {"mixDuration", state.mixDuration}, {"stopOn", state.stopOn}});
  }
  for (size_t i = 0; i < physics_.bodies.size(); ++i) {
    const auto& b = physics_.bodies[i];
    bodies.push_back({{"id", rig_->physics[i].id}, {"ready", b.ready}, {"steps", b.steps},
      {"ox", b.ox}, {"oy", b.oy}, {"vx", b.vx}, {"vy", b.vy}, {"r", b.r}, {"vr", b.vr},
      {"s", b.s}, {"vs", b.vs}, {"px", b.px}, {"py", b.py}, {"tipX", b.tipX}, {"tipY", b.tipY}});
  }
  const auto expr = expression();
  return {{"version", 2}, {"timeScale", timeScale_}, {"paused", paused_}, {"tracks", tracks},
    {"skin", skin()}, {"expression", expr ? Json(*expr) : Json()}, {"nextSerial", nextSerial_},
    {"physics", {{"clock", physics_.clock}, {"wind", physics_.wind}, {"gravity", physics_.gravity},
      {"steps", physics_.steps}, {"settles", physics_.settles}, {"teleports", physics_.teleports}, {"bodies", bodies}}}};
}

void Instance::restorePresentationState(const Json& state) {
  tracks_.clear(); pending_.clear();
  timeScale_ = number(state, "timeScale", 0);
  paused_ = state.at("paused").get<bool>();
  nextSerial_ = counter(state, "nextSerial");
  const auto entry = [this](const Json& j) {
    Entry e;
    e.clip = rig_->clipIndex(j.at("clip").get<std::string>());
    if (e.clip < 0) throw std::runtime_error("Animation checkpoint clip is missing");
    e.loop = j.at("loop").get<bool>();
    e.timeMs = number(j, "timeMs", 0);
    e.speed = number(j, "speed", 1e-3);
    const auto then = j.at("then").get<std::string>();
    if (then != "hold" && then != "stop" && then != "default") throw std::runtime_error("Invalid animation checkpoint: then");
    e.then = then == "hold" ? Then::Hold : then == "stop" ? Then::Stop : Then::Default;
    e.binding = j.at("binding").get<std::string>();
    e.serial = counter(j, "serial"); e.loops = counter(j, "loops");
    if (e.serial >= nextSerial_) throw std::runtime_error("Invalid animation checkpoint serial");
    e.ended = j.at("ended").get<bool>();
    return e;
  };
  const auto& tracks = state.at("tracks");
  if (!tracks.is_array() || tracks.size() > 16) throw std::runtime_error("Invalid animation checkpoint tracks");
  for (const auto& t : tracks) {
    const auto track = counter(t, "track");
    if (track > 15 || tracks_.count(int(track))) throw std::runtime_error("Invalid animation checkpoint track");
    TrackState ts;
    if (!t.at("current").is_null()) ts.current = entry(t.at("current"));
    if (!t.at("from").is_null()) ts.from = entry(t.at("from"));
    ts.mixElapsed = number(t, "mixElapsed", 0); ts.mixDuration = number(t, "mixDuration", 0);
    if (ts.from && (ts.mixDuration == 0 || ts.mixElapsed >= ts.mixDuration)) throw std::runtime_error("Invalid animation checkpoint mix");
    ts.stopOn = t.at("stopOn").get<std::vector<std::string>>();
    tracks_[int(track)] = std::move(ts);
  }
  if (!setSkin(state.at("skin").get<std::string>())) throw std::runtime_error("Animation checkpoint skin is missing");
  const auto& expr = state.at("expression");
  if (!setExpression(expr.is_null() ? std::nullopt : std::optional<std::string>(expr.get<std::string>())))
    throw std::runtime_error("Animation checkpoint expression is missing");
  const auto& p = state.at("physics");
  const auto& bodies = p.at("bodies");
  if (!bodies.is_array() || bodies.size() != rig_->physics.size()) throw std::runtime_error("Animation checkpoint physics has changed");
  physics_.clock = number(p, "clock", 0); physics_.wind = number(p, "wind"); physics_.gravity = number(p, "gravity");
  physics_.steps = counter(p, "steps"); physics_.settles = counter(p, "settles"); physics_.teleports = counter(p, "teleports");
  physics_.bodies.resize(bodies.size());
  for (size_t i = 0; i < bodies.size(); ++i) {
    const auto& j = bodies[i]; auto& b = physics_.bodies[i];
    if (j.at("id") != rig_->physics[i].id) throw std::runtime_error("Animation checkpoint physics has changed");
    b.ready = j.at("ready").get<bool>(); b.steps = counter(j, "steps");
    b.ox = number(j, "ox"); b.oy = number(j, "oy"); b.vx = number(j, "vx"); b.vy = number(j, "vy");
    b.r = number(j, "r"); b.vr = number(j, "vr"); b.s = number(j, "s"); b.vs = number(j, "vs");
    b.px = number(j, "px"); b.py = number(j, "py"); b.tipX = number(j, "tipX"); b.tipY = number(j, "tipY");
  }
}
}  // namespace cutout
