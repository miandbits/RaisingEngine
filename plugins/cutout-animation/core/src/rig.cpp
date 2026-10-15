#include "cutout/rig.hpp"
#include "cutout/mesh.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <set>
#include <stdexcept>

namespace cutout {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr int64_t MAX_DURATION_MS = 10 * 60 * 1000;
constexpr double MAX_ABS = 1e6;

struct Checker {
  std::vector<Diagnostic> out;
  void error(const std::string& pointer, const std::string& code, const std::string& message) { out.push_back({code, "error", pointer, message}); }
  void warn(const std::string& pointer, const std::string& code, const std::string& message) { out.push_back({code, "warning", pointer, message}); }
  bool hasErrors() const { for (const auto& d : out) if (d.severity == "error") return true; return false; }
};
std::string esc(const std::string& key) {
  std::string r;
  for (char c : key) { if (c == '~') r += "~0"; else if (c == '/') r += "~1"; else r += c; }
  return r;
}
std::string at(const std::string& base, const std::string& key) { return base + "/" + esc(key); }
std::string at(const std::string& base, size_t index) { return base + "/" + std::to_string(index); }

bool finiteNumber(const Json& v) { return v.is_number() && std::isfinite(v.get<double>()) && std::fabs(v.get<double>()) <= MAX_ABS; }
// Reads an optional numeric field and reports non-finite or out-of-range values.
double number(Checker& c, const Json& obj, const std::string& pointer, const char* key, double fallback) {
  if (!obj.is_object() || !obj.contains(key)) return fallback;
  const auto& v = obj.at(key);
  if (!finiteNumber(v)) { c.error(at(pointer, key), "ANIM_NONFINITE", std::string(key) + " 값은 유한한 숫자여야 합니다."); return fallback; }
  return v.get<double>();
}
std::string text(const Json& obj, const char* key) {
  if (!obj.is_object() || !obj.contains(key) || !obj.at(key).is_string()) return "";
  return obj.at(key).get<std::string>();
}
void checkTransform(Checker& c, const Json& obj, const std::string& pointer) {
  number(c, obj, pointer, "x", 0); number(c, obj, pointer, "y", 0); number(c, obj, pointer, "rotation", 0);
  for (const char* k : {"scaleX", "scaleY"}) {
    const double s = number(c, obj, pointer, k, 1);
    if (s == 0) c.warn(at(pointer, k), "ANIM_SCALE_ZERO", "크기 0은 조각을 보이지 않게 합니다.");
  }
}
/** Editor display colour of a bone: "#rrggbb" or "#rrggbbaa". */
bool validColor(const Json& v) {
  if (!v.is_string()) return false;
  const std::string& t = v.get_ref<const std::string&>();
  if ((t.size() != 7 && t.size() != 9) || t[0] != '#') return false;
  for (size_t i = 1; i < t.size(); i++) if (!std::isxdigit(static_cast<unsigned char>(t[i]))) return false;
  return true;
}
Transform readTransform(const Json& obj) {
  Transform t;
  auto get = [&](const char* k, double f) { return obj.contains(k) && obj.at(k).is_number() ? obj.at(k).get<double>() : f; };
  t.x = get("x", 0); t.y = get("y", 0); t.rotation = get("rotation", 0); t.scaleX = get("scaleX", 1); t.scaleY = get("scaleY", 1);
  return t;
}
// Bezier handles: x must stay in [0,1] so segment time never runs backwards.
/** `kp` builds the key pointer (only called for a diagnostic). */
template <class Pointer>
void checkCurve(Checker& c, const Json& key, const Pointer& kp) {
  const auto p = [&] { return at(kp(), "curve"); };
  if (!key.contains("curve") || !key.at("curve").is_array() || key.at("curve").size() != 4) {
    c.error(p(), "ANIM_CURVE", "bezier 키에는 curve [x1, y1, x2, y2] 숫자 4개가 필요합니다.");
    return;
  }
  for (size_t i = 0; i < 4; i++) {
    const Json& v = key.at("curve")[i];
    if (!v.is_number() || !std::isfinite(v.get<double>())) { c.error(at(p(), i), "ANIM_CURVE", "curve 값은 유한한 숫자여야 합니다."); continue; }
    const double x = v.get<double>();
    if (i % 2 == 0 && (x < 0 || x > 1)) c.error(at(p(), i), "ANIM_CURVE", "curve의 시간 손잡이(x)는 0..1이어야 합니다.");
    if (i % 2 == 1 && (x < CURVE_Y_MIN || x > CURVE_Y_MAX)) c.error(at(p(), i), "ANIM_CURVE", "curve의 값 손잡이(y)는 -4..5이어야 합니다.");
  }
}
/** Event/attachment name: the schema's $defs/name ([A-Za-z0-9][A-Za-z0-9_-]*, <= 64). */
bool eventName(const std::string& n) {
  if (n.empty() || n.size() > 64 || !std::isalnum(static_cast<unsigned char>(n[0]))) return false;
  for (char ch : n) if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_' && ch != '-') return false;
  return true;
}
constexpr int64_t EVENT_INT_LIMIT = 2147483647;
constexpr size_t EVENT_STRING_LIMIT = 1000;
/** int / float / string payload fields of an event definition or key (plugin 1.8.0). */
void checkPayload(Checker& c, const Json& e, const std::string& p) {
  if (!e.is_object()) return;
  if (e.contains("int") && (!e.at("int").is_number_integer() || e.at("int").get<int64_t>() > EVENT_INT_LIMIT || e.at("int").get<int64_t>() < -EVENT_INT_LIMIT))
    c.error(at(p, "int"), "ANIM_EVENT_PAYLOAD", "이벤트 int 값은 32비트 정수입니다.");
  if (e.contains("float") && !finiteNumber(e.at("float"))) c.error(at(p, "float"), "ANIM_EVENT_PAYLOAD", "이벤트 float 값은 유한한 숫자입니다.");
  if (e.contains("string") && (!e.at("string").is_string() || e.at("string").get_ref<const std::string&>().size() > EVENT_STRING_LIMIT))
    c.error(at(p, "string"), "ANIM_EVENT_PAYLOAD", "이벤트 string 값은 1000바이트 이하의 문자열입니다.");
}
const Json& arrayOr(const Json& obj, const char* key) {
  static const Json empty = Json::array();
  return obj.is_object() && obj.contains(key) && obj.at(key).is_array() ? obj.at(key) : empty;
}

/**
 * Mesh fields of one attachment (plugin 1.4.0): counts, index ranges, uv range and weights.
 * Returns the vertex count of a mesh that runtimes can use (0 for regions and broken meshes),
 * which deform keys are checked against. Weights whose sum is off are normalized at load
 * (warning); everything else that a runtime cannot draw safely is an error.
 */
size_t checkMesh(Checker& c, const Json& a, const std::string& p, const std::map<std::string, std::string>& parentOf) {
  const std::string type = text(a, "type");
  if (a.contains("type") && type != "region" && type != "mesh" && type != "clipping") { c.error(at(p, "type"), "ANIM_MESH_TYPE", "첨부 종류(type)는 region, mesh 또는 clipping입니다."); return 0; }
  const bool clipping = type == "clipping";
  if (clipping) {
    // Clipping (plugin 1.12.0): a polygon, no picture. Image fields would silently do nothing.
    for (const char* k : {"uvs", "triangles", "hull", "edges", "imageAssetId", "width", "height", "pivotX", "pivotY", "sequence", "atlas"})
      if (a.contains(k)) { c.warn(at(p, k), "ANIM_CLIP_UNUSED", std::string("클리핑 첨부에는 쓰이지 않는 값입니다: ") + k); break; }
  } else if (type != "mesh") {
    for (const char* k : {"vertices", "uvs", "triangles", "hull", "edges", "weights"})
      if (a.contains(k)) { c.warn(at(p, k), "ANIM_MESH_UNUSED", std::string("region 첨부의 메시 값은 쓰이지 않습니다: ") + k); break; }
    return 0;
  }
  bool ok = true;
  auto numbers = [&](const char* key, size_t& count) -> const Json* {
    if (!a.contains(key) || !a.at(key).is_array()) { c.error(at(p, key), "ANIM_MESH_" + std::string(key == std::string("uvs") ? "UVS" : "VERTICES"), std::string(key) + "는 숫자 배열입니다."); ok = false; return nullptr; }
    const Json& list = a.at(key);
    for (size_t i = 0; i < list.size(); i++)
      if (!finiteNumber(list[i])) { c.error(at(at(p, key), i), "ANIM_NONFINITE", std::string(key) + " 값은 유한한 숫자여야 합니다."); ok = false; return nullptr; }
    count = list.size();
    return &list;
  };
  size_t coords = 0, uvCount = 0;
  if (clipping && !a.contains("vertices")) { c.error(at(p, "vertices"), "ANIM_CLIP_VERTICES", "클리핑 첨부에는 다각형 정점(vertices)이 필요합니다."); return 0; }
  numbers("vertices", coords);
  const size_t n = coords / 2;
  if (clipping) {
    if (ok && (coords % 2 || n < 3 || n > MAX_CLIP_VERTICES)) {
      c.error(at(p, "vertices"), "ANIM_CLIP_VERTICES", "클리핑 다각형 정점은 x, y 쌍으로 3.." + std::to_string(MAX_CLIP_VERTICES) + "개입니다.");
      ok = false;
    } else if (ok) {
      // Self-intersecting clip polygons have no inside/outside every runtime would agree on.
      std::vector<double> polygon;
      for (const auto& v : a.at("vertices")) polygon.push_back(v.get<double>());
      if (const std::string problem = polygonProblem(polygon); !problem.empty()) { c.error(at(p, "vertices"), "ANIM_CLIP_POLYGON", "클리핑 다각형이 올바르지 않습니다: " + problem); ok = false; }
    }
  } else if (ok && (coords % 2 || n < 3 || n > MAX_MESH_VERTICES)) {
    c.error(at(p, "vertices"), "ANIM_MESH_VERTICES", "메시 정점은 x, y 쌍으로 3.." + std::to_string(MAX_MESH_VERTICES) + "개입니다.");
    ok = false;
  }
  if (clipping) {}
  else if (const Json* uvs = numbers("uvs", uvCount); uvs && ok) {
    if (uvCount != coords) { c.error(at(p, "uvs"), "ANIM_MESH_UVS", "uvs 개수가 정점 수와 다릅니다(정점마다 u, v)."); ok = false; }
    else
      for (size_t i = 0; i < uvCount; i++)
        if ((*uvs)[i].get<double>() < 0 || (*uvs)[i].get<double>() > 1) { c.error(at(at(p, "uvs"), i), "ANIM_MESH_UVS", "uv 값은 0..1입니다."); ok = false; break; }
  }
  auto indexList = [&](const char* key, size_t group, size_t maxGroups, const char* code) {
    const Json& list = a.at(key);
    if (!list.is_array() || list.size() % group || (maxGroups && (list.empty() || list.size() / group > maxGroups))) {
      c.error(at(p, key), code, std::string(key) + (group == 3 ? "는 정점 번호 3개씩(삼각형) 1.." + std::to_string(MAX_MESH_TRIANGLES) + "개입니다." : "는 정점 번호 2개씩의 배열입니다."));
      return false;
    }
    for (size_t i = 0; i < list.size(); i++)
      if (!list[i].is_number_integer() || list[i].get<int64_t>() < 0 || (ok && list[i].get<int64_t>() >= int64_t(n))) {
        c.error(at(at(p, key), i), code, std::string(key) + "의 정점 번호가 범위(0.." + std::to_string(n ? n - 1 : 0) + ")를 벗어났습니다.");
        return false;
      }
    return true;
  };
  if (clipping) {}
  else if (!a.contains("triangles")) { c.error(at(p, "triangles"), "ANIM_MESH_TRIANGLES", "메시에는 triangles(삼각형 정점 번호)가 필요합니다."); ok = false; }
  else if (!indexList("triangles", 3, MAX_MESH_TRIANGLES, "ANIM_MESH_TRIANGLES")) ok = false;
  const bool edgesOk = !clipping && a.contains("edges") && indexList("edges", 2, 0, "ANIM_MESH_EDGES");  // editor-only outline: never blocks drawing
  if (clipping) {}
  else if (a.contains("hull") && (!a.at("hull").is_number_integer() || a.at("hull").get<int64_t>() < 0 || a.at("hull").get<int64_t>() > int64_t(n)))
    c.error(at(p, "hull"), "ANIM_MESH_HULL", "hull(외곽 정점 수)은 0..정점 수 정수입니다.");
  else if (ok && a.contains("hull")) {
    // Outline the mesh edits (plugin 1.6.0) re-triangulate from: warnings only, drawing never uses it.
    const size_t hull = size_t(a.at("hull").get<int64_t>());
    if (hull > 0 && hull < 3) c.warn(at(p, "hull"), "ANIM_MESH_HULL", "윤곽(hull)은 정점 3개 이상이어야 메시를 다시 나눌 수 있습니다.");
    else if (hull >= 3) {
      std::vector<double> outline;
      for (size_t i = 0; i < 2 * hull; i++) outline.push_back(a.at("vertices")[i].get<double>());
      if (const std::string problem = polygonProblem(outline); !problem.empty())
        c.warn(at(p, "hull"), "ANIM_MESH_HULL_SELF", "윤곽(hull)이 올바른 다각형이 아닙니다: " + problem + " 다시 나누기 전에 정점을 옮기세요.");
    }
  }
  if (ok && edgesOk)
    for (size_t i = 0; i + 1 < a.at("edges").size(); i += 2)
      if (a.at("edges")[i] == a.at("edges")[i + 1]) { c.warn(at(at(p, "edges"), i), "ANIM_MESH_EDGES", "가장자리가 같은 정점을 잇습니다(무시됨)."); break; }
  if (a.contains("weights")) {
    const Json& weights = a.at("weights");
    if (!weights.is_array() || (ok && weights.size() != n)) { c.error(at(p, "weights"), "ANIM_MESH_WEIGHTS", "weights는 정점마다 [{bone, weight}] 목록 하나씩입니다(정점 수와 같아야 함)."); return 0; }
    bool warned = false;
    for (size_t v = 0; v < weights.size(); v++) {
      const auto wp = [&] { return at(at(p, "weights"), v); };
      const Json& list = weights[v];
      if (!list.is_array() || list.empty()) { c.error(wp(), "ANIM_MESH_WEIGHTS", "정점마다 본 영향이 1개 이상 필요합니다."); ok = false; continue; }
      if (list.size() > MAX_INFLUENCES) { c.error(wp(), "ANIM_MESH_INFLUENCES", "정점 하나에 영향을 주는 본은 최대 4개입니다."); ok = false; continue; }
      double sum = 0;
      std::vector<std::string> bones;  // <= 4 influences: a set per vertex would allocate for nothing
      for (size_t k = 0; k < list.size(); k++) {
        const std::string bone = text(list[k], "bone");
        if (!parentOf.count(bone)) { c.error(at(at(wp(), k), "bone"), "ANIM_MESH_BONE_MISSING", "메시 웨이트가 없는 본을 가리킵니다: " + bone); ok = false; }
        else if (std::find(bones.begin(), bones.end(), bone) != bones.end()) { c.error(at(at(wp(), k), "bone"), "ANIM_MESH_WEIGHTS", "한 정점에 같은 본이 두 번 있습니다: " + bone); ok = false; }
        else bones.push_back(bone);
        const Json w = list[k].is_object() && list[k].contains("weight") ? list[k].at("weight") : Json();
        if (!finiteNumber(w) || w.get<double>() < 0 || w.get<double>() > 1) { c.error(at(at(wp(), k), "weight"), "ANIM_MESH_WEIGHTS", "웨이트는 0..1 숫자입니다."); ok = false; }
        else sum += w.get<double>();
      }
      if (ok && !(sum > 1e-9)) { c.error(wp(), "ANIM_MESH_WEIGHTS", "정점의 웨이트 합이 0입니다."); ok = false; }
      else if (ok && std::fabs(sum - 1) > 1e-3 && !warned) {
        c.warn(wp(), "ANIM_MESH_WEIGHT_SUM", "웨이트 합이 1이 아니라 불러올 때 1로 맞춥니다(합 " + std::to_string(sum) + ").");
        warned = true;  // one warning per mesh is enough to point at the problem
      }
    }
  }
  return ok ? n : 0;
}

/**
 * Atlas region of one attachment (plugin 1.9.0, written by the export packer): a page id and a
 * sub-image rectangle; `trim` (regions only) must lie inside the attachment image. Meshes keep
 * their whole picture (uvs address it), so a trimmed mesh region is refused.
 */
void checkRegion(Checker& c, const Json& a, const Json& r, const std::string& ap);
void checkAtlas(Checker& c, const Json& a, const std::string& p) {
  if (!a.is_object() || !a.contains("atlas")) return;
  checkRegion(c, a, a.at("atlas"), at(p, "atlas"));
}
/** One atlas region `r` (at pointer `ap`) of attachment `a`: an attachment's own or a sequence frame's. */
void checkRegion(Checker& c, const Json& a, const Json& r, const std::string& ap) {
  if (!r.is_object()) { c.error(ap, "ANIM_ATLAS", "atlas는 {page, x, y, width, height} 객체입니다."); return; }
  const std::string page = text(r, "page");
  if (page.empty() || page.size() > 64) c.error(at(ap, "page"), "ANIM_ATLAS", "atlas.page는 이미지 에셋 ID입니다.");
  for (const char* k : {"x", "y"}) if (number(c, r, ap, k, 0) < 0) c.error(at(ap, k), "ANIM_ATLAS", "atlas 영역 좌표는 0 이상입니다.");
  for (const char* k : {"width", "height"}) if (!(number(c, r, ap, k, 0) > 0)) c.error(at(ap, k), "ANIM_ATLAS", "atlas 영역 크기는 0보다 커야 합니다.");
  if (!r.contains("trim")) return;
  const Json& t = r.at("trim");
  const std::string tp = at(ap, "trim");
  if (text(a, "type") == "mesh") { c.error(tp, "ANIM_ATLAS", "메시 첨부의 atlas 영역은 그림 전체여야 합니다(trim 불가)."); return; }
  if (!t.is_object()) { c.error(tp, "ANIM_ATLAS", "atlas.trim은 {x, y, width, height} 객체입니다."); return; }
  const double x = number(c, t, tp, "x", 0), y = number(c, t, tp, "y", 0), w = number(c, t, tp, "width", 0), h = number(c, t, tp, "height", 0);
  // The attachment size itself is reported by checkAttachment; here it only bounds the trim.
  const double aw = finiteNumber(a.value("width", Json())) ? a.at("width").get<double>() : 0, ah = finiteNumber(a.value("height", Json())) ? a.at("height").get<double>() : 0, eps = 1e-6;
  if (x < 0 || y < 0 || !(w > 0) || !(h > 0) || x + w > aw + eps || y + h > ah + eps)
    c.error(tp, "ANIM_ATLAS", "atlas.trim은 첨부 그림(0..width, 0..height) 안의 영역입니다.");
}

/**
 * Image sequence of one attachment (plugin 1.12.0): frames (image asset ids, frame 0 =
 * imageAssetId), setupIndex and the export-time per-frame atlas regions. Returns the frame count
 * sequence keys are checked against (0 = no usable sequence). Equal frame sizes are a property of
 * the PNG files: the service checks them (ANIM_SEQUENCE_FRAME_SIZE).
 */
size_t checkSequence(Checker& c, const Json& a, const std::string& p) {
  if (!a.is_object() || !a.contains("sequence")) return 0;
  const Json& q = a.at("sequence");
  const std::string sp = at(p, "sequence");
  if (text(a, "type") == "clipping") { c.error(sp, "ANIM_SEQUENCE_TYPE", "클리핑 첨부에는 시퀀스를 쓸 수 없습니다."); return 0; }
  if (!q.is_object()) { c.error(sp, "ANIM_SEQUENCE_FRAMES", "sequence는 {frames, setupIndex?} 객체입니다."); return 0; }
  const Json frames = q.contains("frames") ? q.at("frames") : Json();
  if (!frames.is_array() || frames.empty() || frames.size() > MAX_SEQUENCE_FRAMES) {
    c.error(at(sp, "frames"), "ANIM_SEQUENCE_FRAMES", "시퀀스 프레임(frames)은 이미지 에셋 ID 1.." + std::to_string(MAX_SEQUENCE_FRAMES) + "개입니다.");
    return 0;
  }
  for (size_t i = 0; i < frames.size(); i++)
    if (!frames[i].is_string() || frames[i].get_ref<const std::string&>().empty() || frames[i].get_ref<const std::string&>().size() > 64) {
      c.error(at(at(sp, "frames"), i), "ANIM_SEQUENCE_FRAMES", "시퀀스 프레임은 이미지 에셋 ID입니다.");
      return 0;
    }
  // Frame 0 is the attachment's own picture: tools that know nothing of sequences still show it.
  if (text(a, "imageAssetId") != frames[0].get<std::string>())
    c.error(at(p, "imageAssetId"), "ANIM_SEQUENCE_IMAGE", "시퀀스 첨부의 imageAssetId는 첫 프레임(frames[0])과 같아야 합니다.");
  const size_t n = frames.size();
  if (q.contains("setupIndex") && (!q.at("setupIndex").is_number_integer() || q.at("setupIndex").get<int64_t>() < 0 || q.at("setupIndex").get<int64_t>() >= int64_t(n)))
    c.error(at(sp, "setupIndex"), "ANIM_SEQUENCE_INDEX", "기본 프레임(setupIndex)은 0.." + std::to_string(n - 1) + " 정수입니다.");
  if (q.contains("atlas")) {
    const Json& list = q.at("atlas");
    if (!list.is_array() || list.size() != n) c.error(at(sp, "atlas"), "ANIM_ATLAS", "sequence.atlas는 프레임마다 영역(또는 null) 하나씩입니다.");
    else
      for (size_t i = 0; i < n; i++)
        if (!list[i].is_null()) checkRegion(c, a, list[i], at(at(sp, "atlas"), i));
  }
  for (auto it = q.begin(); it != q.end(); ++it)
    if (it.key() != "frames" && it.key() != "setupIndex" && it.key() != "atlas") { c.warn(at(sp, it.key()), "ANIM_SEQUENCE_UNUSED", "시퀀스에 쓰이지 않는 값입니다: " + it.key()); break; }
  return n;
}

/**
 * IK constraints (plugin 1.3.0): ids, chain shape, target placement, mix and dependencies.
 * Chains of 3+ bones (plugin 1.13.0) follow the two-bone rules: each bone the direct child of the
 * previous one, an explicit tip a direct child of the last; stretch needs bone lengths (warning).
 * A constraint moves every strict descendant of its chain bones; B depends on A when A moves
 * B's target or B's chain base. A dependency cycle can never be satisfied by any order
 * (ANIM_IK_CYCLE); an `order` that solves B before A is only a warning (ANIM_IK_ORDER).
 * Returns the ids `ik:<id>` tracks may use.
 */
std::set<std::string> checkIk(Checker& c, const Json& data, std::map<std::string, std::string>& parentOf, const std::map<std::string, double>& lengthOf) {
  std::set<std::string> ids;
  if (data.contains("ik") && !data.at("ik").is_array()) c.error("/ik", "ANIM_STRUCTURE", "ik는 배열입니다.");
  const auto& list = arrayOr(data, "ik");
  // Strict ancestor test with a guard: a cyclic hierarchy is reported by the bone checks.
  auto under = [&](std::string b, const std::string& ancestor) {
    size_t guard = 0;
    for (b = parentOf.count(b) ? parentOf[b] : ""; !b.empty() && guard++ <= parentOf.size(); b = parentOf.count(b) ? parentOf[b] : "")
      if (b == ancestor) return true;
    return false;
  };
  struct Info { std::vector<std::string> chain; std::string target; int64_t order = 0; bool ok = false; };
  std::vector<Info> info(list.size());
  for (size_t i = 0; i < list.size(); i++) {
    const Json& k = list[i];
    const std::string p = at("/ik", i), id = text(k, "id");
    if (!k.is_object()) { c.error(p, "ANIM_STRUCTURE", "IK 제약은 객체입니다."); continue; }
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "IK 제약 ID가 필요합니다.");
    else if (!ids.insert(id).second) c.error(at(p, "id"), "ANIM_IK_DUP_ID", "중복된 IK 제약 ID: " + id);
    bool ok = true;
    const Json bones = k.contains("bones") ? k.at("bones") : Json();
    if (!bones.is_array() || bones.empty() || bones.size() > MAX_IK_CHAIN) {
      c.error(at(p, "bones"), "ANIM_IK_CHAIN", "IK 사슬은 본 1개(조준), 2개(팔·다리) 또는 3.." + std::to_string(MAX_IK_CHAIN) + "개(부모 → 자식이 이어진 사슬)입니다.");
      ok = false;
    }
    else
      for (size_t b = 0; b < bones.size(); b++) {
        const std::string bone = bones[b].is_string() ? bones[b].get<std::string>() : "";
        if (!parentOf.count(bone)) { c.error(at(at(p, "bones"), b), "ANIM_IK_BONE_MISSING", "IK 사슬이 없는 본을 가리킵니다: " + bone); ok = false; }
        info[i].chain.push_back(bone);
      }
    for (size_t b = 1; ok && b < info[i].chain.size(); b++)
      if (parentOf[info[i].chain[b]] != info[i].chain[b - 1]) {
        c.error(at(p, "bones"), "ANIM_IK_CHAIN", std::string(info[i].chain.size() == 2 ? "두 본 IK 사슬은" : "IK 사슬의 본은 앞 본의") + " 직접 부모 → 자식이어야 합니다: " + info[i].chain[b - 1] + " → " + info[i].chain[b]);
        ok = false;
      }
    // Explicit effector (plugin 1.4.0): picks the hand among several children of the forearm.
    if (k.contains("tip")) {
      const std::string tip = text(k, "tip");
      if (!k.at("tip").is_string() || !parentOf.count(tip)) { c.error(at(p, "tip"), "ANIM_IK_TIP", "IK 끝점(tip) 본이 없습니다: " + tip); ok = false; }
      else if (ok && info[i].chain.size() < 2) { c.error(at(p, "tip"), "ANIM_IK_TIP", "끝점(tip)은 본 2개 이상의 IK에만 씁니다(조준 IK는 x축이 대상을 향합니다)."); ok = false; }
      else if (ok && parentOf[tip] != info[i].chain.back()) { c.error(at(p, "tip"), "ANIM_IK_TIP", "끝점(tip)은 마지막 본 「" + info[i].chain.back() + "」의 직접 자식이어야 합니다: " + tip); ok = false; }
    }
    info[i].target = text(k, "target");
    if (!parentOf.count(info[i].target)) { c.error(at(p, "target"), "ANIM_IK_BONE_MISSING", "IK 대상 본이 없습니다: " + info[i].target); ok = false; }
    else if (ok)
      for (const auto& bone : info[i].chain)
        if (info[i].target == bone || under(info[i].target, bone)) {
          // Solving rotates the chain: a target inside it (or hanging from it) would chase itself.
          c.error(at(p, "target"), "ANIM_IK_TARGET", "IK 대상은 사슬의 본이나 그 자손일 수 없습니다: " + info[i].target);
          ok = false;
          break;
        }
    const double mix = number(c, k, p, "mix", 1);
    if (mix < 0 || mix > 1) c.error(at(p, "mix"), "ANIM_IK_MIX", "IK 혼합(mix)은 0..1이어야 합니다.");
    // Aim axis (plugin 1.4.1): which bone-local direction aims. Two-bone chains solve from the
    // bone positions (elbow, wrist), so an axis there would be silently meaningless.
    if (k.contains("aimAxis")) {
      const Json& axis = k.at("aimAxis");
      if (!axis.is_number() || !std::isfinite(axis.get<double>()) || std::fabs(axis.get<double>()) > 360) {
        c.error(at(p, "aimAxis"), "ANIM_IK_AIM_AXIS", "조준 축(aimAxis)은 -360..360 사이의 각도(도)입니다.");
        ok = false;
      } else if (ok && info[i].chain.size() != 1) {
        c.error(at(p, "aimAxis"), "ANIM_IK_AIM_AXIS", "조준 축(aimAxis)은 조준 IK(본 1개)에만 씁니다. 두 본 IK는 본 위치(팔꿈치·손목)로 풉니다.");
        ok = false;
      }
    }
    for (const char* flag : {"bendPositive", "stretch", "compress", "uniform"})
      if (k.contains(flag) && !k.at(flag).is_boolean()) c.error(at(p, flag), "ANIM_STRUCTURE", std::string(flag) + "는 true 또는 false입니다.");
    // Stretch (plugin 1.13.0): the chain scales along its bones' length axes, so it needs lengths.
    const bool stretch = k.value("stretch", false) == true, compress = k.value("compress", false) == true;
    if (ok && compress && info[i].chain.size() != 1) c.warn(at(p, "compress"), "ANIM_IK_COMPRESS", "줄이기(compress)는 조준 IK(본 1개)에만 쓰입니다. 무시됩니다.");
    if (ok && (stretch || compress)) {
      std::string pointless;
      for (const auto& bone : info[i].chain)
        if (!(lengthOf.count(bone) && lengthOf.at(bone) > 0)) { pointless = bone; break; }
      if (!pointless.empty())
        c.warn(at(p, stretch ? "stretch" : "compress"), "ANIM_IK_STRETCH_LENGTH", "늘이기는 사슬 본의 길이(length) 방향으로 늘이므로 모든 본에 길이가 필요합니다. 길이 없는 본 「" + pointless + "」 때문에 늘이지 않습니다.");
      else if (info[i].chain.size() == 1 && k.contains("aimAxis") && k.at("aimAxis").is_number() && k.at("aimAxis").get<double>() != 0)
        c.warn(at(p, stretch ? "stretch" : "compress"), "ANIM_IK_STRETCH_AXIS", "늘이기는 본 길이 방향(+x)으로 조준할 때만 쓰입니다. 조준 축(aimAxis)이 0이 아니어서 늘이지 않습니다.");
    }
    if (k.value("uniform", false) == true && !stretch && !compress) c.warn(at(p, "uniform"), "ANIM_IK_UNIFORM", "uniform은 늘이기(stretch)·줄이기(compress)와 함께 쓰입니다.");
    if (k.contains("order") && (!k.at("order").is_number_integer() || std::llabs(k.at("order").get<int64_t>()) > 1000))
      c.error(at(p, "order"), "ANIM_STRUCTURE", "IK 순서(order)는 -1000..1000 정수입니다.");
    else info[i].order = k.value("order", int64_t(0));
    info[i].ok = ok;
  }
  // dep[b] = constraints that move b's target or chain base (b must be solved after them).
  const size_t n = list.size();
  std::vector<std::vector<size_t>> dep(n);
  for (size_t b = 0; b < n; b++)
    for (size_t a = 0; a < n; a++) {
      if (a == b || !info[a].ok || !info[b].ok) continue;
      bool moves = false;
      for (const auto& bone : info[a].chain) moves = moves || under(info[b].target, bone) || under(info[b].chain[0], bone);
      if (moves) dep[b].push_back(a);
    }
  std::vector<int> state(n, 0);  // 0 new, 1 on the stack, 2 done
  std::set<size_t> cyclic;
  std::vector<size_t> stack;
  std::function<void(size_t)> visit = [&](size_t v) {
    state[v] = 1;
    stack.push_back(v);
    for (size_t w : dep[v]) {
      if (state[w] == 1) cyclic.insert(std::find(stack.begin(), stack.end(), w), stack.end());
      else if (state[w] == 0) visit(w);
    }
    stack.pop_back();
    state[v] = 2;
  };
  for (size_t v = 0; v < n; v++) if (state[v] == 0) visit(v);
  for (size_t v : cyclic) c.error(at("/ik", v), "ANIM_IK_CYCLE", "IK 제약끼리 서로의 대상·사슬을 움직여 순환합니다: " + text(list[v], "id"));
  auto solvedBefore = [&](size_t a, size_t b) { return info[a].order < info[b].order || (info[a].order == info[b].order && a < b); };
  for (size_t b = 0; b < n; b++)
    for (size_t a : dep[b])
      if (!cyclic.count(b) && solvedBefore(b, a))
        c.warn(at(at("/ik", b), "order"), "ANIM_IK_ORDER", "IK 「" + text(list[a], "id") + "」가 이 제약의 대상·사슬을 움직입니다. 「" + text(list[b], "id") + "」의 order를 더 크게 하세요.");
  return ids;
}

/**
 * Physics constraints (plugin 1.13.0): ids, bone, channel mixes and the simulation parameters.
 * Everything a runtime could not integrate safely (a zero mass, a step outside 1/240..1/10 s,
 * non-finite numbers) is an error; constraints that cannot do anything are warnings.
 * Returns the ids `physics:<id>` tracks may use.
 */
std::set<std::string> checkPhysics(Checker& c, const Json& data, const std::map<std::string, std::string>& parentOf, const std::map<std::string, double>& lengthOf) {
  std::set<std::string> ids;
  if (data.contains("physics") && !data.at("physics").is_array()) c.error("/physics", "ANIM_STRUCTURE", "physics는 배열입니다.");
  const auto& list = arrayOr(data, "physics");
  std::map<std::string, std::string> boneUsed;
  for (size_t i = 0; i < list.size(); i++) {
    const Json& k = list[i];
    const std::string p = at("/physics", i), id = text(k, "id"), bone = text(k, "bone");
    if (!k.is_object()) { c.error(p, "ANIM_STRUCTURE", "물리 제약은 객체입니다."); continue; }
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "물리 제약 ID가 필요합니다.");
    else if (!ids.insert(id).second) c.error(at(p, "id"), "ANIM_PHYSICS_DUP_ID", "중복된 물리 제약 ID: " + id);
    if (!parentOf.count(bone)) c.error(at(p, "bone"), "ANIM_PHYSICS_BONE", "물리 제약이 없는 본을 가리킵니다: " + bone);
    else if (boneUsed.count(bone)) c.warn(at(p, "bone"), "ANIM_PHYSICS_SAME_BONE", "본 「" + bone + "」에 물리 제약 「" + boneUsed[bone] + "」가 이미 있어 흔들림이 겹칩니다.");
    else boneUsed[bone] = id;
    double channels = 0;
    for (const char* key : {"x", "y", "rotate", "scaleX"}) {
      const double v = number(c, k, p, key, 0);
      if (v < 0 || v > 1) c.error(at(p, key), "ANIM_PHYSICS_RANGE", std::string(key) + " (채널 혼합)은 0..1입니다.");
      channels += v;
    }
    if (k.contains("shearX")) c.warn(at(p, "shearX"), "ANIM_PHYSICS_UNUSED", "shearX는 지원하지 않습니다(x, y, rotate, scaleX만 흔듭니다).");
    for (const char* key : {"inertia", "damping", "mix"}) {
      const double v = number(c, k, p, key, 1);
      if (v < 0 || v > 1) c.error(at(p, key), key == std::string("mix") ? "ANIM_PHYSICS_MIX" : "ANIM_PHYSICS_RANGE", std::string(key) + " 값은 0..1입니다.");
    }
    const double strength = number(c, k, p, "strength", 100), mass = number(c, k, p, "mass", 1);
    if (strength < 0 || strength > 10000) c.error(at(p, "strength"), "ANIM_PHYSICS_RANGE", "strength(복원력)는 0..10000입니다.");
    if (!(mass >= 0.01) || mass > 1000) c.error(at(p, "mass"), "ANIM_PHYSICS_MASS", "mass(질량)는 0.01..1000입니다.");
    for (const char* key : {"wind", "gravity"})
      if (std::fabs(number(c, k, p, key, 0)) > 100000) c.error(at(p, key), "ANIM_PHYSICS_RANGE", std::string(key) + " 값은 -100000..100000(px/s²)입니다.");
    const double limit = number(c, k, p, "limit", 5000), teleport = number(c, k, p, "teleport", 300);
    if (!(limit > 0)) c.error(at(p, "limit"), "ANIM_PHYSICS_RANGE", "limit(전달 속도 한계, px/s)은 0보다 커야 합니다.");
    if (!(teleport > 0)) c.error(at(p, "teleport"), "ANIM_PHYSICS_RANGE", "teleport(순간 이동 거리, px)는 0보다 커야 합니다.");
    // A fixed step outside 1/240..1/10 s: too costly per frame, or too coarse to stay stable.
    const double step = number(c, k, p, "step", 1.0 / 60);
    if (!(step >= 1.0 / 240 - 1e-12) || step > 0.1 + 1e-12) c.error(at(p, "step"), "ANIM_PHYSICS_STEP", "step(고정 시간 간격)은 1/240..1/10초입니다.");
    if (k.contains("resetOnStart") && !k.at("resetOnStart").is_boolean()) c.error(at(p, "resetOnStart"), "ANIM_STRUCTURE", "resetOnStart는 true 또는 false입니다.");
    if (channels == 0) c.warn(p, "ANIM_PHYSICS_CHANNELS", "흔들 채널(x, y, rotate, scaleX)이 하나도 켜져 있지 않아 아무것도 하지 않습니다: " + id);
    else if ((number(c, k, p, "rotate", 0) > 0 || number(c, k, p, "scaleX", 0) > 0) && parentOf.count(bone) && !(lengthOf.count(bone) && lengthOf.at(bone) > 0))
      c.warn(at(p, "bone"), "ANIM_PHYSICS_LENGTH", "회전·늘이기 흔들림은 본 길이를 지렛대로 씁니다. 길이 없는 본 「" + bone + "」은 50px로 계산합니다.");
  }
  return ids;
}
}  // namespace

std::optional<Affine> Affine::inverse() const {
  const double det = a * d - b * c;
  if (std::fabs(det) < 1e-12) return std::nullopt;
  const double ia = d / det, ib = -b / det, ic = -c / det, id = a / det;
  return Affine{ia, ib, ic, id, -(ia * e + ic * f), -(ib * e + id * f)};
}
Affine Affine::compose(double x, double y, double rotationDeg, double sx, double sy) {
  const double r = rotationDeg * PI / 180.0, cs = std::cos(r), sn = std::sin(r);
  return {cs * sx, sn * sx, -sn * sy, cs * sy, x, y};
}

const char* propertyName(Property p) {
  switch (p) {
    case Property::X: return "x"; case Property::Y: return "y"; case Property::Rotation: return "rotation";
    case Property::ScaleX: return "scaleX"; case Property::ScaleY: return "scaleY";
    case Property::Alpha: return "alpha"; case Property::Attachment: return "attachment"; case Property::Expression: return "expression";
    case Property::Mix: return "mix"; case Property::BendPositive: return "bendPositive"; case Property::Deform: return "deform";
    case Property::Sequence: return "sequence";
  }
  return "";
}
const char* sequenceModeName(SequenceMode m) {
  switch (m) {
    case SequenceMode::Hold: return "hold"; case SequenceMode::Once: return "once"; case SequenceMode::Loop: return "loop";
    case SequenceMode::Pingpong: return "pingpong"; case SequenceMode::OnceReverse: return "onceReverse"; case SequenceMode::LoopReverse: return "loopReverse";
    case SequenceMode::PingpongReverse: return "pingpongReverse";
  }
  return "hold";
}
std::optional<SequenceMode> parseSequenceMode(const std::string& n) {
  for (auto m : {SequenceMode::Hold, SequenceMode::Once, SequenceMode::Loop, SequenceMode::Pingpong, SequenceMode::OnceReverse, SequenceMode::LoopReverse, SequenceMode::PingpongReverse})
    if (n == sequenceModeName(m)) return m;
  return std::nullopt;
}
std::optional<Property> parseProperty(const std::string& n) {
  for (auto p : {Property::X, Property::Y, Property::Rotation, Property::ScaleX, Property::ScaleY, Property::Alpha, Property::Attachment, Property::Expression,
                 Property::Mix, Property::BendPositive, Property::Deform, Property::Sequence})
    if (n == propertyName(p)) return p;
  return std::nullopt;
}

int Rig::boneIndex(const std::string& id) const { for (size_t i = 0; i < bones.size(); i++) if (bones[i].id == id) return int(i); return -1; }
int Rig::slotIndex(const std::string& id) const { for (size_t i = 0; i < slots.size(); i++) if (slots[i].id == id) return int(i); return -1; }
int Rig::clipIndex(const std::string& id) const { for (size_t i = 0; i < clips.size(); i++) if (clips[i].id == id) return int(i); return -1; }
int Rig::skinIndex(const std::string& id) const { for (size_t i = 0; i < skins.size(); i++) if (skins[i].id == id) return int(i); return -1; }
int Rig::expressionIndex(const std::string& id) const { for (size_t i = 0; i < expressions.size(); i++) if (expressions[i].id == id) return int(i); return -1; }
int Rig::ikIndex(const std::string& id) const { for (size_t i = 0; i < ik.size(); i++) if (ik[i].id == id) return int(i); return -1; }
int Rig::physicsIndex(const std::string& id) const { for (size_t i = 0; i < physics.size(); i++) if (physics[i].id == id) return int(i); return -1; }
int Rig::eventIndex(const std::string& name) const { for (size_t i = 0; i < events.size(); i++) if (events[i].name == name) return int(i); return -1; }
const Attachment* Rig::attachment(const std::string& slot, const std::string& name, int skin) const {
  if (skin >= 0 && skin < int(skins.size()))
    for (const auto& a : skins[size_t(skin)].attachments) if (a.slot == slot && a.name == name) return &a;
  for (const auto& a : attachments) if (a.slot == slot && a.name == name) return &a;
  return nullptr;
}
int64_t Rig::mixDuration(const std::string& from, const std::string& to) const {
  for (const auto& m : mixes) if (m.from == from && m.to == to) return m.durationMs;
  return defaultMixMs;
}

std::vector<Diagnostic> validateRig(const Json& data) {
  Checker c;
  if (!data.is_object()) { c.error("", "ANIM_STRUCTURE", "리그 데이터는 객체여야 합니다."); return c.out; }
  if (!data.contains("schemaVersion") || data.at("schemaVersion") != SCHEMA_VERSION)
    c.error("/schemaVersion", "ANIM_SCHEMA_VERSION", "지원하는 리그 schemaVersion은 1입니다.");
  if (data.contains("canvas")) {
    const auto& canvas = data.at("canvas");
    for (const char* k : {"width", "height"}) if (number(c, canvas, "/canvas", k, 1) <= 0) c.error(at("/canvas", k), "ANIM_CANVAS_SIZE", "캔버스 크기는 0보다 커야 합니다.");
    number(c, canvas, "/canvas", "originX", 0); number(c, canvas, "/canvas", "originY", 0);
  }

  // Bones: unique ids, known parents, no cycles.
  const auto& bones = arrayOr(data, "bones");
  std::map<std::string, std::string> parentOf;
  std::map<std::string, size_t> boneAt;
  std::map<std::string, double> lengthOf;  // valid lengths only (IK stretch, physics levers)
  for (size_t i = 0; i < bones.size(); i++) {
    const std::string p = at("/bones", i), id = text(bones[i], "id");
    if (id.empty()) { c.error(at(p, "id"), "ANIM_ID_MISSING", "본 ID가 필요합니다."); continue; }
    if (boneAt.count(id)) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 본 ID: " + id);
    boneAt[id] = i;
    parentOf[id] = bones[i].contains("parent") && bones[i].at("parent").is_string() ? bones[i].at("parent").get<std::string>() : "";
    checkTransform(c, bones[i], p);
    // Length (plugin 1.5.0): display + two-bone effector only, so a generous but finite range.
    if (bones[i].contains("length")) {
      const Json& v = bones[i].at("length");
      if (!v.is_number() || !std::isfinite(v.get<double>()) || v.get<double>() < 0 || v.get<double>() > 100000)
        c.error(at(p, "length"), "ANIM_BONE_LENGTH", "본 길이(length)는 0..100000 사이의 숫자(px)입니다.");
      else lengthOf[id] = v.get<double>();
    }
    if (bones[i].contains("color") && !validColor(bones[i].at("color")))
      c.error(at(p, "color"), "ANIM_BONE_COLOR", "본 색(color)은 \"#rrggbb\" 또는 \"#rrggbbaa\" 형식입니다.");
  }
  if (bones.empty()) c.error("/bones", "ANIM_NO_BONES", "본이 하나 이상 필요합니다.");
  for (const auto& [id, parent] : parentOf) {
    if (parent.empty()) continue;
    const std::string p = at(at("/bones", boneAt[id]), "parent");
    if (!parentOf.count(parent)) { c.error(p, "ANIM_PARENT_MISSING", "없는 부모 본을 참조합니다: " + parent); continue; }
    std::set<std::string> seen{id};
    for (std::string cur = parent; !cur.empty(); cur = parentOf.count(cur) ? parentOf[cur] : "") {
      if (!seen.insert(cur).second) { c.error(p, "ANIM_PARENT_CYCLE", "부모 관계가 순환합니다: " + id); break; }
    }
  }

  const std::set<std::string> ikIds = checkIk(c, data, parentOf, lengthOf);
  const std::set<std::string> physicsIds = checkPhysics(c, data, parentOf, lengthOf);

  // Slots and attachments.
  const auto& slots = arrayOr(data, "slots");
  std::set<std::string> slotIds;
  std::map<std::string, size_t> slotOrder;  // draw order index (clipping ranges, plugin 1.12.0)
  for (size_t i = 0; i < slots.size(); i++) {
    const std::string p = at("/slots", i), id = text(slots[i], "id"), bone = text(slots[i], "bone");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "슬롯 ID가 필요합니다.");
    else if (!slotIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 슬롯 ID: " + id);
    else slotOrder[id] = i;
    if (!parentOf.count(bone)) c.error(at(p, "bone"), "ANIM_BONE_MISSING", "슬롯이 없는 본을 참조합니다: " + bone);
    const double alpha = number(c, slots[i], p, "alpha", 1);
    if (alpha < 0 || alpha > 1) c.error(at(p, "alpha"), "ANIM_ALPHA_RANGE", "투명도는 0..1이어야 합니다.");
  }
  using SlotName = std::pair<std::string, std::string>;
  std::map<SlotName, std::set<size_t>> meshCounts;  // vertex counts of the usable meshes (and clip polygons) named (slot, name), any list
  std::map<std::string, std::set<size_t>> sequenceCounts;  // slot -> frame counts of its sequence attachments (plugin 1.12.0)
  // One attachment entry (default list or a skin's list); `seen` holds the list's (slot, name) pairs.
  auto checkAttachment = [&](const Json& a, const std::string& p, std::set<SlotName>& seen) {
    if (const size_t n = checkMesh(c, a, p, parentOf)) meshCounts[{text(a, "slot"), text(a, "name")}].insert(n);
    const std::string slot = text(a, "slot"), name = text(a, "name");
    if (!slotIds.count(slot)) c.error(at(p, "slot"), "ANIM_SLOT_MISSING", "첨부가 없는 슬롯을 참조합니다: " + slot);
    if (name.empty()) c.error(at(p, "name"), "ANIM_ID_MISSING", "첨부 이름이 필요합니다.");
    else if (!seen.insert({slot, name}).second) c.error(at(p, "name"), "ANIM_DUP_ID", "같은 슬롯에 중복된 첨부 이름: " + name);
    checkTransform(c, a, p);
    if (text(a, "type") == "clipping") {
      // Clip range (plugin 1.12.0): from the next slot through `end`, which must come later in draw order.
      const std::string end = text(a, "end");
      if (!a.contains("end") || end.empty()) c.error(at(p, "end"), "ANIM_CLIP_END", "클리핑 첨부에는 끝 슬롯(end)이 필요합니다.");
      else if (!slotOrder.count(end)) c.error(at(p, "end"), "ANIM_CLIP_END", "클리핑 끝 슬롯이 없습니다: " + end);
      else if (slotOrder.count(slot) && slotOrder[end] <= slotOrder[slot]) c.error(at(p, "end"), "ANIM_CLIP_END", "클리핑 끝 슬롯은 그리는 순서에서 클리핑 슬롯 「" + slot + "」보다 뒤(위)에 있어야 합니다: " + end);
      return;
    }
    if (a.contains("end")) c.warn(at(p, "end"), "ANIM_CLIP_UNUSED", "end는 클리핑 첨부에만 씁니다.");
    checkAtlas(c, a, p);
    if (const size_t frames = checkSequence(c, a, p)) sequenceCounts[slot].insert(frames);
    if (text(a, "imageAssetId").empty()) c.error(at(p, "imageAssetId"), "ANIM_IMAGE_MISSING", "첨부에 이미지가 지정되지 않았습니다.");
    for (const char* k : {"pivotX", "pivotY"}) number(c, a, p, k, 0.5);
    for (const char* k : {"width", "height"}) if (number(c, a, p, k, 0) <= 0) c.error(at(p, k), "ANIM_IMAGE_SIZE", "이미지 크기는 0보다 커야 합니다.");
  };
  const auto& attachments = arrayOr(data, "attachments");
  std::set<SlotName> attachmentIds;
  for (size_t i = 0; i < attachments.size(); i++) checkAttachment(attachments[i], at("/attachments", i), attachmentIds);

  // Skins (plugin 1.2.0): per-skin attachment lists keyed like the default list.
  const auto& skins = arrayOr(data, "skins");
  std::set<std::string> skinIds;
  std::vector<std::set<SlotName>> skinNames(skins.size());
  std::set<SlotName> anySkin;  // (slot, name) provided by at least one skin
  for (size_t i = 0; i < skins.size(); i++) {
    const std::string p = at("/skins", i), id = text(skins[i], "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "스킨 ID가 필요합니다.");
    else if (!skinIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 스킨 ID: " + id);
    const auto& list = arrayOr(skins[i], "attachments");
    for (size_t k = 0; k < list.size(); k++) checkAttachment(list[k], at(at(p, "attachments"), k), skinNames[i]);
    anySkin.insert(skinNames[i].begin(), skinNames[i].end());
  }
  // A name is usable when the default list or any skin provides it (an outfit-only part).
  auto knownName = [&](const std::string& slot, const std::string& name) { return attachmentIds.count({slot, name}) || anySkin.count({slot, name}); };
  std::set<SlotName> referenced;  // names that setup/keys/expressions show: every skin should resolve them
  for (size_t i = 0; i < slots.size(); i++) {
    if (!slots[i].contains("attachment") || !slots[i].at("attachment").is_string()) continue;
    const auto name = slots[i].at("attachment").get<std::string>();
    if (!knownName(text(slots[i], "id"), name)) c.error(at(at("/slots", i), "attachment"), "ANIM_ATTACHMENT_UNKNOWN", "슬롯 기본 첨부가 없습니다: " + name);
    else referenced.insert({text(slots[i], "id"), name});
  }

  // Expressions (plugin 1.2.0): {id, name, slots: {slot: name|null}, bones: {bone: {channel: value}}}.
  const auto& expressions = arrayOr(data, "expressions");
  std::set<std::string> expressionIds;
  for (size_t i = 0; i < expressions.size(); i++) {
    const auto& e = expressions[i];
    const std::string p = at("/expressions", i), id = text(e, "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "표정 ID가 필요합니다.");
    else if (!expressionIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 표정 ID: " + id);
    if (e.is_object() && e.contains("slots") && e.at("slots").is_object())
      for (auto it = e.at("slots").begin(); it != e.at("slots").end(); ++it) {
        const std::string sp = at(at(p, "slots"), it.key());
        if (!slotIds.count(it.key())) { c.error(sp, "ANIM_SLOT_MISSING", "표정이 없는 슬롯을 참조합니다: " + it.key()); continue; }
        if (it.value().is_null()) continue;
        if (!it.value().is_string()) c.error(sp, "ANIM_KEY_VALUE", "표정의 슬롯 값은 첨부 이름 또는 null입니다.");
        else if (!knownName(it.key(), it.value().get<std::string>())) c.error(sp, "ANIM_ATTACHMENT_UNKNOWN", "슬롯에 없는 첨부를 가리킵니다: " + it.value().get<std::string>());
        else referenced.insert({it.key(), it.value().get<std::string>()});
      }
    else if (e.is_object() && e.contains("slots")) c.error(at(p, "slots"), "ANIM_STRUCTURE", "표정 slots는 객체입니다.");
    if (e.is_object() && e.contains("bones") && e.at("bones").is_object())
      for (auto it = e.at("bones").begin(); it != e.at("bones").end(); ++it) {
        const std::string bp = at(at(p, "bones"), it.key());
        if (!parentOf.count(it.key())) { c.error(bp, "ANIM_BONE_MISSING", "표정이 없는 본을 참조합니다: " + it.key()); continue; }
        if (!it.value().is_object()) { c.error(bp, "ANIM_STRUCTURE", "표정의 본 값은 {rotation, x, y, scaleX, scaleY} 객체입니다."); continue; }
        for (auto ch = it.value().begin(); ch != it.value().end(); ++ch) {
          const auto prop = parseProperty(ch.key());
          if (!prop || (*prop != Property::X && *prop != Property::Y && *prop != Property::Rotation && *prop != Property::ScaleX && *prop != Property::ScaleY))
            c.error(at(bp, ch.key()), "ANIM_PROPERTY", "표정의 본 속성은 x, y, rotation, scaleX, scaleY입니다: " + ch.key());
          else if (!finiteNumber(ch.value())) c.error(at(bp, ch.key()), "ANIM_NONFINITE", ch.key() + " 값은 유한한 숫자여야 합니다.");
        }
      }
    else if (e.is_object() && e.contains("bones")) c.error(at(p, "bones"), "ANIM_STRUCTURE", "표정 bones는 객체입니다.");
  }

  // Event definitions (plugin 1.8.0): once any exist, every clip event key must name one.
  const auto& eventDefs = arrayOr(data, "events");
  std::set<std::string> eventNames;
  for (size_t i = 0; i < eventDefs.size(); i++) {
    const auto& e = eventDefs[i];
    const std::string p = at("/events", i), name = text(e, "name");
    if (!e.is_object()) { c.error(p, "ANIM_STRUCTURE", "이벤트 정의는 객체입니다."); continue; }
    if (!eventName(name)) c.error(at(p, "name"), "ANIM_EVENT_NAME", "이벤트 이름은 영문·숫자로 시작하고 영문·숫자·_·-만 씁니다(64자 이하): " + name);
    else if (!eventNames.insert(name).second) c.error(at(p, "name"), "ANIM_DUP_ID", "중복된 이벤트 이름: " + name);
    checkPayload(c, e, p);
    if (e.contains("audio") && (!e.at("audio").is_string() || e.at("audio").get_ref<const std::string&>().empty() || e.at("audio").get_ref<const std::string&>().size() > 64))
      c.error(at(p, "audio"), "ANIM_EVENT_AUDIO", "이벤트 소리(audio)는 오디오 에셋 ID입니다.");
    const double volume = number(c, e, p, "volume", 1), balance = number(c, e, p, "balance", 0);
    if (volume < 0 || volume > 1) c.error(at(p, "volume"), "ANIM_EVENT_PAYLOAD", "이벤트 음량(volume)은 0..1입니다.");
    if (balance < -1 || balance > 1) c.error(at(p, "balance"), "ANIM_EVENT_PAYLOAD", "이벤트 좌우(balance)는 -1..1입니다.");
    if (!e.contains("audio") && (e.contains("volume") || e.contains("balance")))
      c.warn(p, "ANIM_EVENT_AUDIO_UNUSED", "소리(audio)가 없는 이벤트의 음량·좌우 값은 쓰이지 않습니다: " + name);
  }

  // Clips.
  const auto& clips = arrayOr(data, "clips");
  std::set<std::string> clipIds;
  for (size_t ci = 0; ci < clips.size(); ci++) {
    const auto& clip = clips[ci];
    const std::string p = at("/clips", ci), id = text(clip, "id");
    if (id.empty()) c.error(at(p, "id"), "ANIM_ID_MISSING", "클립 ID가 필요합니다.");
    else if (!clipIds.insert(id).second) c.error(at(p, "id"), "ANIM_DUP_ID", "중복된 클립 ID: " + id);
    int64_t duration = 0;
    if (!clip.contains("durationMs") || !clip.at("durationMs").is_number_integer() || clip.at("durationMs").get<int64_t>() <= 0 || clip.at("durationMs").get<int64_t>() > MAX_DURATION_MS)
      c.error(at(p, "durationMs"), "ANIM_DURATION", "클립 길이는 1..600000ms 정수여야 합니다.");
    else duration = clip.at("durationMs").get<int64_t>();
    const auto& tracks = arrayOr(clip, "tracks");
    std::set<std::string> targets;
    for (size_t ti = 0; ti < tracks.size(); ti++) {
      const auto& track = tracks[ti];
      const std::string tp = at(at(p, "tracks"), ti), target = text(track, "target"), propertyText = text(track, "property");
      const auto property = parseProperty(propertyText);
      const bool isBone = target.rfind("bone:", 0) == 0, isSlot = target.rfind("slot:", 0) == 0, isRig = target == "rig", isIk = target.rfind("ik:", 0) == 0;
      const bool isPhysics = target.rfind("physics:", 0) == 0;
      const std::string targetId = isIk ? target.substr(3) : isPhysics ? target.substr(8) : target.size() > 5 ? target.substr(5) : "";
      if (!property) { c.error(at(tp, "property"), "ANIM_PROPERTY", "알 수 없는 트랙 속성: " + propertyText); continue; }
      const bool deform = *property == Property::Deform, sequence = *property == Property::Sequence;
      const bool slotProperty = *property == Property::Alpha || *property == Property::Attachment || deform || sequence;
      const bool ikProperty = *property == Property::Mix || *property == Property::BendPositive;
      if (isRig != (*property == Property::Expression))
        c.error(at(tp, "property"), "ANIM_PROPERTY", isRig ? "rig 트랙의 속성은 expression뿐입니다." : "expression 속성은 rig 트랙에만 씁니다.");
      else if (isRig) {}
      else if (isPhysics) {
        // Physics tracks (plugin 1.13.0) key the constraint's mix only.
        if (*property != Property::Mix) c.error(at(tp, "property"), "ANIM_PROPERTY", "물리 트랙의 속성은 mix뿐입니다.");
        else if (!physicsIds.count(targetId)) c.error(at(tp, "target"), "ANIM_TARGET_MISSING", "트랙이 없는 물리 제약을 가리킵니다: " + target);
      }
      else if (isIk != ikProperty)
        c.error(at(tp, "property"), "ANIM_PROPERTY", isIk ? "IK 트랙의 속성은 mix, bendPositive입니다." : "mix·bendPositive 속성은 IK 트랙(ik:<id>)에만 씁니다.");
      else if (isIk) { if (!ikIds.count(targetId)) c.error(at(tp, "target"), "ANIM_TARGET_MISSING", "트랙이 없는 IK 제약을 가리킵니다: " + target); }
      else if ((!isBone && !isSlot) || (isBone && !parentOf.count(targetId)) || (isSlot && !slotIds.count(targetId)))
        c.error(at(tp, "target"), "ANIM_TARGET_MISSING", "트랙 대상이 없습니다: " + target);
      else if (isBone == slotProperty) c.error(at(tp, "property"), "ANIM_PROPERTY", "대상 종류에 맞지 않는 속성입니다: " + propertyText);
      // Deform tracks (plugin 1.4.0) key one mesh attachment of the slot by name: offsets must fit
      // the vertex count of the meshes with that name (any list; others with another count ignore them).
      const std::string deformName = deform ? text(track, "attachment") : "";
      const std::set<size_t>* deformCounts = nullptr;
      if (deform && isSlot) {
        auto found = meshCounts.find({targetId, deformName});
        if (deformName.empty()) c.error(at(tp, "attachment"), "ANIM_MESH_DEFORM", "변형(deform) 트랙에는 메시 첨부 이름(attachment)이 필요합니다.");
        else if (found == meshCounts.end()) c.error(at(tp, "attachment"), "ANIM_MESH_DEFORM", "변형 트랙이 메시가 아닌(또는 없는) 첨부를 가리킵니다: " + targetId + "/" + deformName);
        else {
          deformCounts = &found->second;
          if (deformCounts->size() > 1) c.warn(at(tp, "attachment"), "ANIM_MESH_DEFORM_SKIN", "같은 이름의 메시들의 정점 수가 달라, 키와 정점 수가 맞는 메시에만 변형이 적용됩니다: " + deformName);
        }
      } else if (track.contains("attachment")) c.warn(at(tp, "attachment"), "ANIM_MESH_UNUSED", "attachment는 변형(deform) 트랙에만 씁니다.");
      // Sequence tracks (plugin 1.12.0) pick frames of whichever sequence attachment the slot shows.
      size_t maxFrames = 0;
      if (sequence && isSlot && slotIds.count(targetId)) {
        if (!sequenceCounts.count(targetId)) c.error(at(tp, "target"), "ANIM_SEQUENCE_TARGET", "시퀀스 트랙의 슬롯에 시퀀스 첨부가 없습니다: " + targetId);
        else maxFrames = *sequenceCounts[targetId].rbegin();
      }
      if (!targets.insert(target + "#" + propertyText + (deform ? "#" + deformName : "")).second) c.error(tp, "ANIM_DUP_TRACK", "같은 대상·속성 트랙이 중복됩니다.");
      const auto& keys = arrayOr(track, "keys");
      if (keys.empty()) c.warn(at(tp, "keys"), "ANIM_EMPTY_TRACK", "키가 없는 트랙입니다.");
      int64_t previous = -1;
      for (size_t ki = 0; ki < keys.size(); ki++) {
        const auto& key = keys[ki];
        // Pointers are built only for a diagnostic: thousands of valid keys must not allocate them.
        const auto kp = [&] { return at(at(tp, "keys"), ki); };
        if (!key.contains("timeMs") || !key.at("timeMs").is_number_integer()) { c.error(at(kp(), "timeMs"), "ANIM_KEY_TIME", "키 시간은 정수 ms여야 합니다."); continue; }
        const int64_t t = key.at("timeMs").get<int64_t>();
        if (t < 0 || (duration > 0 && t > duration)) c.error(at(kp(), "timeMs"), "ANIM_KEY_TIME_RANGE", "키 시간이 클립 범위(0..길이)를 벗어났습니다.");
        if (t <= previous) c.error(at(kp(), "timeMs"), "ANIM_KEY_UNSORTED", "키는 시간순으로 중복 없이 정렬되어야 합니다.");
        previous = t;
        const std::string interp = text(key, "interp");
        if (!interp.empty() && interp != "step" && interp != "linear" && interp != "bezier") c.error(at(kp(), "interp"), "ANIM_INTERP", "보간은 step, linear, bezier 중 하나입니다.");
        else if (interp == "bezier" && (*property == Property::Attachment || *property == Property::Expression || *property == Property::BendPositive))
          c.error(at(kp(), "interp"), "ANIM_INTERP", "첨부·표정·IK 굽힘 키는 항상 계단(step) 전환입니다.");
        else if (sequence && !interp.empty() && interp != "step") c.error(at(kp(), "interp"), "ANIM_INTERP", "시퀀스 키는 항상 계단(step) 전환입니다.");
        if (interp == "bezier") checkCurve(c, key, kp);
        else if (key.contains("curve")) c.warn(at(kp(), "curve"), "ANIM_CURVE_UNUSED", "bezier가 아닌 키의 curve는 쓰이지 않습니다.");
        if (sequence) {
          // Step keys {mode, index, delay}; the frame follows from the clip time (Spine semantics).
          if (key.contains("mode") && (!key.at("mode").is_string() || !parseSequenceMode(key.at("mode").get<std::string>())))
            c.error(at(kp(), "mode"), "ANIM_SEQUENCE_KEY", "시퀀스 재생 방식(mode)은 hold, once, loop, pingpong, onceReverse, loopReverse, pingpongReverse 중 하나입니다.");
          if (key.contains("index") && (!key.at("index").is_number_integer() || key.at("index").get<int64_t>() < 0 || (maxFrames && key.at("index").get<int64_t>() >= int64_t(maxFrames))))
            c.error(at(kp(), "index"), "ANIM_SEQUENCE_INDEX", "시퀀스 프레임 번호(index)는 0.." + std::to_string(maxFrames ? maxFrames - 1 : 0) + " 정수입니다.");
          if (key.contains("delay") && (!finiteNumber(key.at("delay")) || !(key.at("delay").get<double>() > 0) || key.at("delay").get<double>() > 60000))
            c.error(at(kp(), "delay"), "ANIM_SEQUENCE_DELAY", "시퀀스 프레임 간격(delay)은 0보다 크고 60000ms 이하입니다.");
          if (key.contains("value")) c.warn(at(kp(), "value"), "ANIM_SEQUENCE_UNUSED", "시퀀스 키는 value 대신 mode·index·delay를 씁니다.");
        } else if (deform) {
          const Json offsets = key.contains("offsets") ? key.at("offsets") : Json();
          bool finite = offsets.is_array();
          for (size_t i = 0; finite && i < offsets.size(); i++) finite = finiteNumber(offsets[i]);
          if (!finite) c.error(at(kp(), "offsets"), "ANIM_MESH_DEFORM", "변형 키에는 offsets(정점마다 dx, dy 숫자)가 필요합니다.");
          else if (deformCounts && (offsets.size() % 2 || !deformCounts->count(offsets.size() / 2)))
            c.error(at(kp(), "offsets"), "ANIM_MESH_DEFORM", "변형 키의 offsets 개수가 메시 정점 수 × 2와 다릅니다: " + std::to_string(offsets.size()));
        } else if (*property == Property::BendPositive) {
          if (!key.contains("value") || !key.at("value").is_boolean()) c.error(at(kp(), "value"), "ANIM_KEY_VALUE", "IK 굽힘 키 값은 true 또는 false입니다.");
        } else if (*property == Property::Expression) {
          if (!key.contains("value") || (!key.at("value").is_null() && !key.at("value").is_string())) c.error(at(kp(), "value"), "ANIM_KEY_VALUE", "표정 키 값은 표정 ID 또는 null입니다.");
          else if (key.at("value").is_string() && !expressionIds.count(key.at("value").get<std::string>()))
            c.error(at(kp(), "value"), "ANIM_EXPRESSION_UNKNOWN", "없는 표정을 가리킵니다: " + key.at("value").get<std::string>());
        } else if (*property == Property::Attachment) {
          if (!key.contains("value") || (!key.at("value").is_null() && !key.at("value").is_string())) c.error(at(kp(), "value"), "ANIM_KEY_VALUE", "첨부 키 값은 이름 또는 null입니다.");
          else if (key.at("value").is_string() && !knownName(targetId, key.at("value").get<std::string>()))
            c.error(at(kp(), "value"), "ANIM_ATTACHMENT_UNKNOWN", "슬롯에 없는 첨부를 가리킵니다: " + key.at("value").get<std::string>());
          else if (key.at("value").is_string()) referenced.insert({targetId, key.at("value").get<std::string>()});
        } else if (!key.contains("value") || !finiteNumber(key.at("value"))) c.error(at(kp(), "value"), "ANIM_NONFINITE", "키 값은 유한한 숫자여야 합니다.");
        else if (*property == Property::Alpha && (key.at("value").get<double>() < 0 || key.at("value").get<double>() > 1)) c.error(at(kp(), "value"), "ANIM_ALPHA_RANGE", "투명도는 0..1이어야 합니다.");
        else if (*property == Property::Mix && (key.at("value").get<double>() < 0 || key.at("value").get<double>() > 1))
          c.error(at(kp(), "value"), isPhysics ? "ANIM_PHYSICS_MIX" : "ANIM_IK_MIX", isPhysics ? "물리 혼합(mix)은 0..1이어야 합니다." : "IK 혼합(mix)은 0..1이어야 합니다.");
      }
    }
    const auto& events = arrayOr(clip, "events");
    std::set<std::pair<int64_t, std::string>> seenEvents;
    for (size_t ei = 0; ei < events.size(); ei++) {
      const std::string ep = at(at(p, "events"), ei), name = text(events[ei], "name");
      const bool timed = events[ei].is_object() && events[ei].contains("timeMs") && events[ei].at("timeMs").is_number_integer();
      if (!timed || events[ei].at("timeMs").get<int64_t>() < 0 || (duration > 0 && events[ei].at("timeMs").get<int64_t>() > duration))
        c.error(at(ep, "timeMs"), "ANIM_EVENT_TIME", "이벤트 시간이 클립 범위(0..길이, 정수 ms)를 벗어났습니다.");
      if (name.empty()) { c.error(at(ep, "name"), "ANIM_ID_MISSING", "이벤트 이름이 필요합니다."); continue; }
      if (!eventNames.empty() && !eventNames.count(name))
        c.error(at(ep, "name"), "ANIM_EVENT_UNKNOWN", "정의되지 않은 이벤트입니다(리그 이벤트 목록에 추가하세요): " + name);
      checkPayload(c, events[ei], ep);
      // Two keys of one name at one time both fire; editing ops address the first of them.
      if (timed && !seenEvents.insert({events[ei].at("timeMs").get<int64_t>(), name}).second)
        c.warn(ep, "ANIM_EVENT_DUPLICATE", "같은 시간에 같은 이벤트가 두 번 있습니다: " + name);
    }
  }
  // A skin that cannot resolve a shown name (neither it nor the default list has it) draws
  // nothing there while active: allowed (a deliberately bare part), but worth a warning.
  for (size_t i = 0; i < skins.size(); i++)
    for (const auto& [slot, name] : referenced)
      if (!attachmentIds.count({slot, name}) && !skinNames[i].count({slot, name}))
        c.warn(at("/skins", i), "ANIM_SKIN_MISSING_ATTACHMENT", "스킨 「" + text(skins[i], "id") + "」에 쓰이는 첨부가 없습니다(그 자리는 비어 보입니다): " + slot + "/" + name);
  if (data.contains("mix") && data.at("mix").is_object()) {
    const auto& mix = data.at("mix");
    if (mix.contains("defaultMs") && (!mix.at("defaultMs").is_number_integer() || mix.at("defaultMs").get<int64_t>() < 0 || mix.at("defaultMs").get<int64_t>() > 10000))
      c.error("/mix/defaultMs", "ANIM_MIX", "기본 전환 시간은 0..10000ms입니다.");
    const auto& pairs = arrayOr(mix, "pairs");
    for (size_t i = 0; i < pairs.size(); i++) {
      const std::string pp = at("/mix/pairs", i);
      for (const char* k : {"from", "to"}) if (!clipIds.count(text(pairs[i], k))) c.error(at(pp, k), "ANIM_CLIP_UNKNOWN", "전환 표가 없는 클립을 참조합니다: " + text(pairs[i], k));
      if (!pairs[i].contains("durationMs") || !pairs[i].at("durationMs").is_number_integer() || pairs[i].at("durationMs").get<int64_t>() < 0 || pairs[i].at("durationMs").get<int64_t>() > 10000)
        c.error(at(pp, "durationMs"), "ANIM_MIX", "전환 시간은 0..10000ms입니다.");
    }
  }
  return c.out;
}

Rig parseRig(const std::string& id, const Json& data) {
  for (const auto& d : validateRig(data))
    if (d.severity == "error") throw std::runtime_error(d.code + " " + d.pointer + ": " + d.message);
  return parseValidatedRig(id, data);
}

Rig parseValidatedRig(const std::string& id, const Json& data) {
  Rig rig;
  rig.id = id;
  if (data.contains("canvas")) {
    const auto& cv = data.at("canvas");
    rig.width = cv.value("width", 0.0); rig.height = cv.value("height", 0.0);
    rig.originX = cv.value("originX", 0.0); rig.originY = cv.value("originY", 0.0);
  }
  for (const auto& b : data.at("bones")) {
    Bone bone; bone.id = b.at("id"); bone.setup = readTransform(b);
    if (b.contains("parent") && b.at("parent").is_string()) bone.parent = b.at("parent");
    bone.length = b.value("length", 0.0);
    bone.color = b.value("color", std::string());
    rig.bones.push_back(bone);
  }
  for (auto& b : rig.bones) b.parentIndex = b.parent.empty() ? -1 : rig.boneIndex(b.parent);
  // Parents-first evaluation order; validation guarantees no cycles.
  std::vector<char> placed(rig.bones.size(), 0);
  while (rig.evalOrder.size() < rig.bones.size())
    for (size_t i = 0; i < rig.bones.size(); i++)
      if (!placed[i] && (rig.bones[i].parentIndex < 0 || placed[rig.bones[i].parentIndex])) { placed[i] = 1; rig.evalOrder.push_back(int(i)); }
  for (const auto& s : arrayOr(data, "slots")) {
    Slot slot; slot.id = s.at("id"); slot.bone = s.at("bone"); slot.alpha = s.value("alpha", 1.0);
    if (s.contains("attachment") && s.at("attachment").is_string()) slot.attachment = s.at("attachment").get<std::string>();
    slot.boneIndex = rig.boneIndex(slot.bone);
    rig.slots.push_back(slot);
  }
  // Bind pose of weighted meshes: forward kinematics of the setup transforms (no IK, no expression).
  std::vector<Affine> setupWorld(rig.bones.size());
  for (int i : rig.evalOrder) {
    const auto& t = rig.bones[size_t(i)].setup;
    const Affine local = Affine::compose(t.x, t.y, t.rotation, t.scaleX, t.scaleY);
    const int p = rig.bones[size_t(i)].parentIndex;
    setupWorld[size_t(i)] = p < 0 ? local : setupWorld[size_t(p)] * local;
  }
  auto readRegion = [](const Json& r) {
    AtlasRegion region;
    region.page = r.at("page"); region.x = r.value("x", 0.0); region.y = r.value("y", 0.0); region.width = r.at("width"); region.height = r.at("height");
    if (r.contains("trim") && r.at("trim").is_object()) {
      const Json& t = r.at("trim");
      region.trimmed = true;
      region.trimX = t.value("x", 0.0); region.trimY = t.value("y", 0.0); region.trimWidth = t.at("width"); region.trimHeight = t.at("height");
    }
    return region;
  };
  auto readAttachment = [&](const Json& a) {
    const std::string type = a.value("type", std::string());
    Attachment att; att.slot = a.at("slot"); att.name = a.at("name"); att.imageAssetId = a.value("imageAssetId", std::string());
    att.offset = readTransform(a); att.pivotX = a.value("pivotX", 0.5); att.pivotY = a.value("pivotY", 0.5);
    att.width = a.value("width", 0.0); att.height = a.value("height", 0.0);
    if (type == "clipping") {
      // No picture: the placement is the offset alone (no size, no pivot), vertices in that space.
      att.clipping = true;
      att.pivotX = att.pivotY = 0;
      att.width = att.height = 0;
      att.clipEnd = a.at("end").get<std::string>();
      att.clipEndIndex = rig.slotIndex(att.clipEnd);
    }
    if (!att.clipping && a.contains("atlas") && a.at("atlas").is_object()) att.atlas = readRegion(a.at("atlas"));
    if (!att.clipping && a.contains("sequence") && a.at("sequence").is_object()) {
      const Json& q = a.at("sequence");
      for (const auto& f : q.at("frames")) att.frames.push_back(f.get<std::string>());
      att.setupIndex = q.value("setupIndex", 0);
      if (q.contains("atlas") && q.at("atlas").is_array())
        for (const auto& r : q.at("atlas")) att.frameAtlas.push_back(r.is_object() ? std::optional<AtlasRegion>(readRegion(r)) : std::nullopt);
    }
    if (type != "mesh" && !att.clipping) return att;
    att.mesh = !att.clipping;
    for (const auto& v : a.at("vertices")) att.vertices.push_back(v.get<double>());
    if (att.mesh) {
      for (const auto& v : a.at("uvs")) att.uvs.push_back(v.get<double>());
      for (const auto& v : a.at("triangles")) att.triangles.push_back(v.get<int>());
      if (a.contains("hull")) att.hull = a.at("hull").get<size_t>();
      if (a.contains("edges")) for (const auto& v : a.at("edges")) att.edges.push_back(v.get<int>());
    }
    if (!a.contains("weights")) return att;
    const int slot = rig.slotIndex(att.slot);
    const int slotBone = slot >= 0 ? rig.slots[size_t(slot)].boneIndex : -1;
    const Affine placed = (slotBone >= 0 ? setupWorld[size_t(slotBone)] : Affine{}) * Affine::compose(att.offset.x, att.offset.y, att.offset.rotation, att.offset.scaleX, att.offset.scaleY) *
                          Affine::compose(-att.pivotX * att.width, -att.pivotY * att.height, 0, 1, 1);
    std::map<int, int> bindOf;  // bone index -> index in att.binds
    for (const auto& list : a.at("weights")) {
      std::vector<Influence> influences;
      double sum = 0;
      for (const auto& w : list) {
        const int bone = rig.boneIndex(w.at("bone").get<std::string>());
        const auto inverse = setupWorld[size_t(bone)].inverse();
        // A zero-scale setup bone cannot be bound (ANIM_SCALE_ZERO warns); it simply gets no share.
        if (!inverse || !(w.at("weight").get<double>() > 0)) continue;
        if (!bindOf.count(bone)) { bindOf[bone] = int(att.binds.size()); att.binds.push_back({bone, *inverse * placed}); }
        influences.push_back({bindOf[bone], w.at("weight").get<double>()});
        sum += influences.back().weight;
      }
      for (auto& x : influences) x.weight /= sum;  // ANIM_MESH_WEIGHT_SUM: normalized here
      att.weights.push_back(influences);
    }
    return att;
  };
  for (const auto& a : arrayOr(data, "attachments")) rig.attachments.push_back(readAttachment(a));
  for (const auto& s : arrayOr(data, "skins")) {
    Skin skin; skin.id = s.at("id"); skin.name = s.value("name", skin.id);
    for (const auto& a : arrayOr(s, "attachments")) skin.attachments.push_back(readAttachment(a));
    rig.skins.push_back(skin);
  }
  for (const auto& e : arrayOr(data, "expressions")) {
    Expression x; x.id = e.at("id"); x.name = e.value("name", x.id);
    if (e.contains("slots") && e.at("slots").is_object())
      for (auto it = e.at("slots").begin(); it != e.at("slots").end(); ++it)
        x.slots.push_back({rig.slotIndex(it.key()), it.value().is_string() ? std::optional<std::string>(it.value().get<std::string>()) : std::nullopt});
    if (e.contains("bones") && e.at("bones").is_object())
      for (auto it = e.at("bones").begin(); it != e.at("bones").end(); ++it) {
        BoneOverride o; o.bone = rig.boneIndex(it.key());
        auto get = [&](const char* k) { return it.value().contains(k) ? std::optional<double>(it.value().at(k).get<double>()) : std::nullopt; };
        o.x = get("x"); o.y = get("y"); o.rotation = get("rotation"); o.scaleX = get("scaleX"); o.scaleY = get("scaleY");
        x.bones.push_back(o);
      }
    rig.expressions.push_back(x);
  }
  for (const auto& k : arrayOr(data, "ik")) {
    IkConstraint x; x.id = k.at("id"); x.name = k.value("name", x.id);
    for (const auto& b : k.at("bones")) x.bones.push_back(rig.boneIndex(b.get<std::string>()));
    x.target = rig.boneIndex(k.at("target").get<std::string>());
    x.mix = k.value("mix", 1.0); x.aimAxis = k.value("aimAxis", 0.0); x.bendPositive = k.value("bendPositive", true); x.stretch = k.value("stretch", false); x.order = k.value("order", 0);
    x.compress = x.bones.size() == 1 && k.value("compress", false);
    x.uniform = k.value("uniform", false);
    x.stretchable = x.bones.size() > 1 || x.aimAxis == 0;
    for (int b : x.bones) x.stretchable = x.stretchable && rig.bones[size_t(b)].length > 0;
    // The effector of a chain of 2+: the explicit `tip` (plugin 1.4.0), else the end bone's own
    // tip when it has a length (plugin 1.5.0), else its first child (array order).
    x.explicitTip = k.contains("tip");
    const int end = x.bones.back();
    if (x.explicitTip) x.tip = rig.boneIndex(k.at("tip").get<std::string>());
    else if (x.bones.size() >= 2 && rig.bones[size_t(end)].length > 0) x.tipLength = rig.bones[size_t(end)].length;
    else if (x.bones.size() >= 2)
      for (size_t i = 0; i < rig.bones.size() && x.tip < 0; i++) if (rig.bones[i].parentIndex == end) x.tip = int(i);
    std::vector<char> inside(rig.bones.size(), 0);
    for (int i : rig.evalOrder) {
      const int p = rig.bones[size_t(i)].parentIndex;
      inside[size_t(i)] = i == x.bones[0] || (p >= 0 && inside[size_t(p)]);
      if (inside[size_t(i)]) x.affected.push_back(i);
    }
    rig.ik.push_back(x);
  }
  for (size_t i = 0; i < rig.ik.size(); i++) rig.ikOrder.push_back(int(i));
  std::stable_sort(rig.ikOrder.begin(), rig.ikOrder.end(), [&](int a, int b) { return rig.ik[size_t(a)].order < rig.ik[size_t(b)].order; });
  std::vector<size_t> depth(rig.bones.size(), 0);  // place in evalOrder: parents before children
  for (size_t i = 0; i < rig.evalOrder.size(); i++) depth[size_t(rig.evalOrder[i])] = i;
  for (const auto& k : arrayOr(data, "physics")) {
    PhysicsConstraint x; x.id = k.at("id"); x.name = k.value("name", x.id);
    x.bone = rig.boneIndex(k.at("bone").get<std::string>());
    x.x = k.value("x", 0.0); x.y = k.value("y", 0.0); x.rotate = k.value("rotate", 0.0); x.scaleX = k.value("scaleX", 0.0);
    x.inertia = k.value("inertia", 1.0); x.strength = k.value("strength", 100.0); x.damping = k.value("damping", 0.85);
    x.mass = k.value("mass", 1.0); x.wind = k.value("wind", 0.0); x.gravity = k.value("gravity", 0.0); x.mix = k.value("mix", 1.0);
    x.limit = k.value("limit", 5000.0); x.step = k.value("step", 1.0 / 60); x.teleport = k.value("teleport", 300.0);
    x.resetOnStart = k.value("resetOnStart", false);
    x.stepMs = x.step * 1000;
    // Once at load (pow is libm): every step then multiplies by the same number in this runtime.
    x.decay = std::pow(x.damping, 60 * x.step);
    if (rig.bones[size_t(x.bone)].length > 0) x.lever = rig.bones[size_t(x.bone)].length;
    std::vector<char> inside(rig.bones.size(), 0);
    for (int i : rig.evalOrder) {
      const int p = rig.bones[size_t(i)].parentIndex;
      inside[size_t(i)] = i == x.bone || (p >= 0 && inside[size_t(p)]);
      if (inside[size_t(i)]) x.affected.push_back(i);
    }
    rig.physics.push_back(x);
  }
  // Parents first: a hair tip on a swinging hair root sees the root's motion of the same step.
  for (size_t i = 0; i < rig.physics.size(); i++) rig.physicsOrder.push_back(int(i));
  std::stable_sort(rig.physicsOrder.begin(), rig.physicsOrder.end(), [&](int a, int b) { return depth[size_t(rig.physics[size_t(a)].bone)] < depth[size_t(rig.physics[size_t(b)].bone)]; });
  for (const auto& e : arrayOr(data, "events")) {
    EventDef d; d.name = e.at("name");
    if (e.contains("int")) d.intValue = e.at("int").get<int64_t>();
    if (e.contains("float")) d.floatValue = e.at("float").get<double>();
    if (e.contains("string")) d.stringValue = e.at("string").get<std::string>();
    if (e.contains("audio")) d.audio = e.at("audio").get<std::string>();
    d.volume = e.value("volume", 1.0); d.balance = e.value("balance", 0.0);
    rig.events.push_back(d);
  }
  for (const auto& c : arrayOr(data, "clips")) {
    Clip clip; clip.id = c.at("id"); clip.name = c.value("name", clip.id); clip.durationMs = c.at("durationMs"); clip.loop = c.value("loop", true);
    for (const auto& t : arrayOr(c, "tracks")) {
      Track track; const std::string target = t.at("target");
      track.rig = target == "rig";
      track.ik = target.rfind("ik:", 0) == 0;
      track.physics = target.rfind("physics:", 0) == 0;
      track.bone = target.rfind("bone:", 0) == 0; track.target = track.rig ? "" : target.substr(track.ik ? 3 : track.physics ? 8 : 5);
      track.targetIndex = track.rig ? -1 : track.ik ? rig.ikIndex(track.target) : track.physics ? rig.physicsIndex(track.target) : track.bone ? rig.boneIndex(track.target) : rig.slotIndex(track.target);
      track.property = *parseProperty(t.at("property"));
      if (track.property == Property::Deform) track.attachment = t.at("attachment").get<std::string>();
      for (const auto& k : arrayOr(t, "keys")) {
        Key key; key.timeMs = k.at("timeMs");
        if (track.property == Property::Sequence) {
          key.interp = Interp::Step;
          key.value = double(k.value("index", int64_t(0)));
          key.mode = parseSequenceMode(k.value("mode", std::string("hold"))).value_or(SequenceMode::Hold);
          key.delay = k.value("delay", 100.0);
        } else if (track.property == Property::Deform) {
          for (const auto& v : k.at("offsets")) key.offsets.push_back(v.get<double>());
          const std::string interp = text(k, "interp");
          key.interp = interp == "step" ? Interp::Step : interp == "bezier" ? Interp::Bezier : Interp::Linear;
          if (key.interp == Interp::Bezier) for (size_t i = 0; i < 4; i++) key.curve[i] = k.at("curve")[i].get<double>();
        } else if (track.property == Property::Attachment || track.property == Property::Expression) { key.interp = Interp::Step; if (k.at("value").is_string()) key.name = k.at("value").get<std::string>(); }
        else if (track.property == Property::BendPositive) { key.interp = Interp::Step; key.value = k.at("value").get<bool>() ? 1 : 0; }
        else {
          key.value = k.at("value");
          const std::string interp = text(k, "interp");
          key.interp = interp == "step" ? Interp::Step : interp == "bezier" ? Interp::Bezier : Interp::Linear;
          if (key.interp == Interp::Bezier) for (size_t i = 0; i < 4; i++) key.curve[i] = k.at("curve")[i].get<double>();
        }
        track.keys.push_back(key);
      }
      clip.tracks.push_back(track);
    }
    for (const auto& e : arrayOr(c, "events")) {
      ClipEvent ev{e.at("timeMs").get<int64_t>(), e.at("name").get<std::string>()};
      // Payload: the definition's defaults, the key's own int/float/string win (Spine semantics).
      const int def = rig.eventIndex(ev.name);
      if (def >= 0) {
        const EventDef& d = rig.events[size_t(def)];
        if (d.intValue) ev.payload["int"] = *d.intValue;
        if (d.floatValue) ev.payload["float"] = *d.floatValue;
        if (d.stringValue) ev.payload["string"] = *d.stringValue;
      }
      for (const char* k : {"int", "float", "string"}) if (e.contains(k)) ev.payload[k] = e.at(k);
      if (def >= 0 && rig.events[size_t(def)].audio) {
        const EventDef& d = rig.events[size_t(def)];
        ev.payload["audio"] = *d.audio; ev.payload["volume"] = d.volume; ev.payload["balance"] = d.balance;
      }
      clip.events.push_back(ev);
    }
    rig.clips.push_back(clip);
  }
  if (data.contains("mix") && data.at("mix").is_object()) {
    rig.defaultMixMs = data.at("mix").value("defaultMs", int64_t(0));
    for (const auto& m : arrayOr(data.at("mix"), "pairs")) rig.mixes.push_back({m.at("from"), m.at("to"), m.at("durationMs").get<int64_t>()});
  }
  return rig;
}
}  // namespace cutout
