#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QSet>

// Pure presentation-only projection. The caller owns the eventual editor transaction.
struct TimelineBatchOptions {
  QSet<QString> selected;
  bool replaceMapped = false;
  QString existingTimelineId; // Empty creates one new timeline per activity.
  QJsonArray clips;           // imageSource: activity, none, or asset:<registered image ID>.
  QString fallbackImage;     // Empty means explicitly text-only; fallbackChosen distinguishes unset.
  bool fallbackChosen = false;
  QString nameSuffix;
};
struct TimelineBatchPlan {
  QJsonArray timelines, bindings, impact;
  QString error;
  int created = 0, linked = 0, skipped = 0;
  bool valid() const { return error.isEmpty(); }
};
inline QString timelineBatchNextId(const QJsonArray &timelines) {
  QSet<QString> used; for (auto value : timelines) used.insert(value.toObject()["id"].toString());
  for (int n = 1;; ++n) { auto id = QString("activity-presentation-%1").arg(n); if (!used.contains(id)) return id; }
}
inline TimelineBatchPlan planTimelineBatch(const QJsonArray &activities, const QJsonArray &timelines,
                                         const QJsonArray &bindings, const QSet<QString> &images,
                                         const TimelineBatchOptions &options) {
  TimelineBatchPlan plan; plan.timelines = timelines; plan.bindings = bindings;
  auto fail = [&](QString message) { plan.error = message; return plan; };
  if (options.selected.isEmpty()) return fail(QObject::tr("활동을 하나 이상 선택하세요."));
  QSet<QString> activityIds, timelineIds;
  for (auto a : activities) activityIds.insert(a.toObject()["id"].toString());
  for (auto t : timelines) timelineIds.insert(t.toObject()["id"].toString());
  for (const auto &id : options.selected) if (!activityIds.contains(id)) return fail(QObject::tr("선택한 활동을 찾을 수 없습니다: ")+id);
  const bool create = options.existingTimelineId.isEmpty();
  if (!create && !timelineIds.contains(options.existingTimelineId)) return fail(QObject::tr("연결할 연출을 선택하세요."));
  if (create && (options.clips.isEmpty() || options.clips.size() > 100)) return fail(QObject::tr("클립은 1개 이상 100개 이하로 설정하세요."));
  if (create && !options.fallbackImage.isEmpty() && !images.contains(options.fallbackImage)) return fail(QObject::tr("대체 이미지를 다시 선택하세요."));
  for (auto value : activities) {
    auto activity = value.toObject(); const auto id = activity["id"].toString();
    if (!options.selected.contains(id)) continue;
    int bindingIndex = -1; QJsonObject binding;
    for (int i = 0; i < plan.bindings.size(); ++i) if (plan.bindings[i].toObject()["activityId"] == id) { bindingIndex = i; binding = plan.bindings[i].toObject(); break; }
    QJsonObject impact{{"activityId",id},{"label",activity["label"]}};
    if (bindingIndex >= 0 && !options.replaceMapped) {
      impact["action"] = "skip"; impact["timelineId"] = binding["timelineId"]; plan.impact.append(impact); ++plan.skipped; continue;
    }
    QString target = options.existingTimelineId;
    if (create) {
      if (plan.timelines.size() >= 500) return fail(QObject::tr("연출은 최대 500개입니다."));
      QJsonArray clips, usedImages; bool fallbackUsed = false;
      for (int i = 0; i < options.clips.size(); ++i) {
        auto source = options.clips[i].toObject(); const auto text = source["text"].toString();
        if (text.size() > 4000) return fail(QObject::tr("클립 문구는 4000자 이내로 입력하세요."));
        QRegularExpression token("\\{([^{}]*)\\}"); auto matches = token.globalMatch(text);
        while (matches.hasNext()) { const auto key = matches.next().captured(1); if (!QStringList{"activity","outcome","day","changes"}.contains(key)) return fail(QObject::tr("지원하지 않는 문구 변수: ")+key); }
        const int duration = source["durationMs"].toInt(); if (duration < 1 || duration > 60000) return fail(QObject::tr("클립 시간은 1~60000ms여야 합니다."));
        auto image = source["imageSource"].toString("activity");
        if (image == "activity") {
          image = activity["presentation"].toObject()["backgroundAssetId"].toString();
          if (image.isEmpty() || !images.contains(image)) {
            if (!options.fallbackChosen) return fail(QObject::tr("활동 배경이 없는 경우 사용할 대체 이미지 또는 문구만 표시를 선택하세요: ")+activity["label"].toString(id));
            image = options.fallbackImage; fallbackUsed = true;
          }
        } else if (image == "none") image.clear();
        else if (image.startsWith("asset:")) image = image.mid(6);
        else return fail(QObject::tr("누락된 이미지를 다시 선택하세요: ")+image);
        if (!image.isEmpty() && !images.contains(image)) return fail(QObject::tr("누락된 이미지를 다시 선택하세요: ")+image);
        if (image.isEmpty() && text.trimmed().isEmpty()) return fail(QObject::tr("각 클립에 이미지 또는 문구를 입력하세요."));
        auto transition = source["transition"].toString("none"); if (transition != "none" && transition != "fade") return fail(QObject::tr("클립 등장 방식을 다시 선택하세요."));
        QJsonObject clip{{"id",QString("clip-%1").arg(i+1)},{"durationMs",duration},{"text",text},{"transition",transition}};
        if (!image.isEmpty()) { clip["imageAssetId"] = image; if (!usedImages.contains(image)) usedImages.append(image); }
        clips.append(clip);
      }
      target = timelineBatchNextId(plan.timelines);
      plan.timelines.append(QJsonObject{{"id",target},{"name",(activity["label"].toString(id)+options.nameSuffix).left(200)},{"clips",clips}});
      impact["images"] = usedImages; impact["fallbackUsed"] = fallbackUsed; ++plan.created;
    }
    if (bindingIndex < 0) binding = QJsonObject{{"activityId",id},{"repeat","once"}};
    // Preserve all existing success/failure/repeat fields, including an omitted repeat.
    binding["timelineId"] = target;
    if (bindingIndex < 0) plan.bindings.append(binding); else plan.bindings[bindingIndex] = binding;
    impact["action"] = create ? "create" : "link"; impact["timelineId"] = target;
    impact["replacesDefault"] = bindingIndex >= 0; plan.impact.append(impact); ++plan.linked;
  }
  return plan;
}
