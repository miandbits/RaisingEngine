#include "calendar.hpp"
#include <QObject>
#include "canvas.hpp"
#include "design.hpp"
#include "surface-paint.hpp"
#include "surface-image.hpp"
#include "components.hpp"
#include "selection-edit.hpp"
#include "component-preview.hpp"
#include "activity-board-preview.hpp"
#include "schedule-preview.hpp"
#include "choice-preview.hpp"
#include <QGraphicsObject>
#include <QDate>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>
#include <cmath>

static QColor rgba(QString value, QColor fallback) {
  if (value.size() == 9) {
    QColor c(value.left(7));
    c.setAlpha(value.right(2).toInt(nullptr, 16));
    return c;
  }
  QColor c(value);
  return c.isValid() ? c : fallback;
}
class VisualItem : public QGraphicsObject {
public:
  QJsonObject data;
  QPixmap image;
  QImage surfaceImage;
  ScreenCanvas *canvas;
  QSize bounds;
  QRectF box;
  QPointF origin;
  QRectF before;
  int corner = -1;
  bool multipleDrag=false;
  bool runtimeLayer=false,runtimeVisible=true;
  QRectF runtimeBase;
  void setRuntimeGeometry(QRectF rect){prepareGeometryChange();setPos(rect.topLeft());box.setSize(rect.size());}
  QRect authoredRectangle(QRectF rectangle) const {
    if(runtimeLayer)rectangle=QRectF(QPointF(data["x"].toDouble(),data["y"].toDouble())+rectangle.topLeft()-runtimeBase.topLeft(),
      QSizeF(data["width"].toDouble(),data["height"].toDouble())+rectangle.size()-runtimeBase.size());
    return rectangle.toRect().translated(-qRound(data.value("_parentX").toDouble()),-qRound(data.value("_parentY").toDouble()));
  }
  VisualItem(QJsonObject value, QPixmap pixmap, ScreenCanvas *view, QSize size)
      : data(value), image(pixmap), canvas(view), bounds(size) {
    box = {0, 0, data["width"].toDouble(), data["height"].toDouble()};
    setPos(data["x"].toDouble(), data["y"].toDouble());
    setData(0, data["id"].toString());
    setFlag(ItemIsSelectable);
    setFlag(ItemIsFocusable);
    setCursor(data["locked"].toBool() ? Qt::ArrowCursor : Qt::SizeAllCursor);
  }
  QRectF boundingRect() const override { return box.adjusted(-6, -6, 6, 6); }
  QPainterPath shape() const override {
    QPainterPath p;
    if(runtimeLayer&&!runtimeVisible&&!isSelected())return p;
    if(data.value("kind")=="group"&&!isSelected()) {p.addRect(box.adjusted(-3,-3,3,3));p.addRect(box.adjusted(3,3,-3,-3));p.setFillRule(Qt::OddEvenFill);}
    else p.addRect(box);
    if(data.contains("_clipRect")){QPainterPath clip;clip.addRect(canvasClipRect(data.value("_clipRect").toObject()).translated(-pos()));return p.intersected(clip);}
    return p;
  }
  void paint(QPainter *p, const QStyleOptionGraphicsItem *,
             QWidget *) override {
    if(runtimeLayer){
      if(isSelected()){
        QPen pen(QColor(runtimeVisible?"#65a6ff":"#e0a040"),2,runtimeVisible?Qt::SolidLine:Qt::DashLine);pen.setCosmetic(true);p->setPen(pen);p->setBrush(Qt::NoBrush);p->drawRect(box);
        if(!data["locked"].toBool())for(auto pt:{box.topLeft(),box.topRight(),box.bottomLeft(),box.bottomRight()}){p->setBrush(Qt::white);p->drawRect(QRectF(pt-QPointF(4,4),QSizeF(8,8)));}
      }
      return;
    }
    p->setRenderHint(QPainter::Antialiasing);
    p->save();
    if(data.contains("_clipRect"))p->setClipRect(canvasClipRect(data.value("_clipRect").toObject()).translated(-pos()));
    p->setOpacity(data["opacity"].toDouble(1));
    const auto kind = data["kind"].toString();
    const auto decoration=[&]{const auto inset=qMax(0.,data.value("borderWidth").toDouble());paintSurface(p,box.adjusted(inset,inset,-inset,-inset),surfaceImage,data.value("surface").toObject());};
    if (kind == "image" && !image.isNull()) {
      decoration();
      const bool contain = data["fit"].toString() == "contain";
      QSizeF scaled = image.size();
      scaled.scale(box.size(), contain ? Qt::KeepAspectRatio
                                       : Qt::KeepAspectRatioByExpanding);
      QRectF target(QPointF((box.width() - scaled.width()) / 2,
                            (box.height() - scaled.height()) / 2),
                    scaled);
      p->setClipRect(box,Qt::IntersectClip);
      p->drawPixmap(target, image, QRectF(image.rect()));
    } else if (kind == "plugin") {
      decoration();paintPlugin(p);
    } else if (kind == "meter") {
      decoration();
      const double thickness=qMin(box.height(),data.value("meterThickness").toDouble(box.height()));
      const QRectF track(0,(box.height()-thickness)/2,box.width(),thickness);
      p->fillRect(track,rgba(data.value("fill").toString(),QColor("#16242e")));
      const double maximum=qMax(0.0001,data.value("maxValue").toDouble(100));
      const double ratio=qBound(0.0,data.value("value").toDouble()/maximum,1.0);
      p->fillRect(QRectF(0,track.y(),box.width()*ratio,thickness),rgba(data.value("color").toString(),QColor("#68c6a0")));
      p->setPen(rgba(data.value("color").toString(),Qt::white));
      p->drawText(box,Qt::AlignCenter,data.value("text").toString());
    } else if (kind == "result" && data.value("resultLayout")=="growth") {
      p->fillRect(box,rgba(data.value("fill").toString(),QColor("#fff9e8")));decoration();QFont font(data["_fontFamily"].toString("Noto Sans KR"));font.setPixelSize(data.value("fontSize").toInt(20));p->setFont(font);p->setPen(rgba(data.value("color").toString(),QColor("#304d46")));
      p->drawText(box.adjusted(12,12,-12,-12),Qt::AlignTop|Qt::TextWordWrap,QObject::tr("이번 활동 성장\n이전 → 이후 · 최종 변화\n실제 확정된 변화는 플레이에서 확인"));
    } else if (kind == "choices") {
      decoration();paintChoicePreview(p,box,data);
    } else if (kind == "schedule" && !data.value("_rosterPreview").toArray().isEmpty()) {
      paintRosterPreview(p,box,data,decoration);
    } else if (kind == "schedule" && data["scheduleLayout"] == "activity-board") {
      paintActivityBoardPreview(p,box,data,decoration);
    } else if (kind == "schedule" && data["scheduleLayout"] == "calendar") {
      p->fillRect(box, QColor("#080706")); decoration();p->setPen(QColor("#cba99d"));
      QFont font(data["_fontFamily"].toString("Noto Sans KR"));font.setPixelSize(data["fontSize"].toInt(24));p->setFont(font);
      const auto project=data.value("_calendarProject").toObject();const auto config=StudioCalendar::spec(project);const auto date=StudioCalendar::date(project,0);const auto grid=raising::dates::grid(config,date,project.value("startYear").toInt(1),project.value("startMonth").toInt(1),qMax(1,project.value("periodsPerMonth").toInt(3)));
      p->drawText(QRectF(0,0,box.width(),36),Qt::AlignCenter,StudioCalendar::label(project,0,false));
      const qreal cw=box.width()/7,ch=(box.height()-80)/grid.rows;
      for(int i=0;i<7;i++)p->drawText(QRectF(i*cw,40,cw,32),Qt::AlignCenter,QString::fromStdString(config.weekdays[i]));
      for(int day=1;day<=grid.days;day++){int cell=grid.offset+day-1;QRectF r((cell%7)*cw,80+(cell/7)*ch,cw,ch);p->drawRect(r);p->drawText(r.adjusted(3,2,-3,-2),Qt::AlignLeft|Qt::AlignTop,QString::number(day));}
    } else if (kind == "schedule" && data["scheduleLayout"] == "menu") {
      decoration();
      auto groups=data["scheduleGroups"].toArray();QRectF r(box.width()-320,210,320,qMax(90,groups.size()*42+32));
      p->fillRect(r,QColor("#46343e"));p->setPen(QColor("#e5c880"));p->drawRect(r);
      QFont font(data["_fontFamily"].toString("Noto Sans KR"));font.setPixelSize(data["fontSize"].toInt(28));p->setFont(font);
      for(int i=0;i<groups.size();i++)p->drawText(QRectF(r.x()+16,r.y()+16+i*42,288,40),Qt::AlignLeft|Qt::AlignVCenter,groups[i].toObject()["label"].toString());
    } else {
      bool simple = kind == "button" || kind == "panel" || kind == "text" || kind == "group" || kind == "input";
      QColor fill =
          rgba(data["fill"].toString(), kind == "button" ? QColor("#355c4c")
                                        : simple ? QColor(Qt::transparent)
                                                 : QColor("#fff9e8"));
      p->setBrush(fill);
      p->setPen(Qt::NoPen);
      if(data["borderWidth"].toDouble()>0)p->setPen(QPen(QColor(data["borderColor"].toString("#57806c")),data["borderWidth"].toDouble()));
      p->drawRoundedRect(box, data["radius"].toInt(6), data["radius"].toInt(6));
      decoration();
      if (kind != "panel" && kind != "group") {
        QString text = data["text"].toString();
        if (text.isEmpty() && !data.contains("text"))
          text = data["label"].toString();
        if (text.isEmpty() && kind == "text" && !data.contains("text"))
          text = data["binding"].toString();
        if (text.isEmpty() && !data.contains("text")) {
          static const QMap<QString, QString> labels = {
              {"locations", QObject::tr("이동할 장소")},
              {"shop", QObject::tr("상품 목록 · 실제 내용은 플레이에서 확인")},
              {"sell", QObject::tr("판매 목록 · 수량 / 확인창")},
              {"services", QObject::tr("병원 · 비용 / 치료 효과")},
              {"actions", QObject::tr("공통 행동 · 조건 / 비용 / 효과")},
              {"policies", QObject::tr("정책 그룹 · 독립 선택 / 실행 주기")},
              {"diets", QObject::tr("지속 식단 · 일정별 비용 / 효과")},
              {"inventory", QObject::tr("소지품")},
              {"stats", QObject::tr("능력 · 자원")},
              {"image", QObject::tr("이미지 없음")},
              {"schedule", QObject::tr("일정 선택")},
              {"dialogue", QObject::tr("대사")}};
          text = labels.value(kind, kind);
        }
        QFont font(data["_fontFamily"].toString("Noto Sans KR"));
        font.setPixelSize(data["fontSize"].toInt(20));
        p->setFont(font);
        p->setPen(rgba(data["color"].toString(),
                       kind == "button" ? Qt::white : QColor("#304d46")));
        const int align = data["align"].toString() == "left" ? Qt::AlignLeft
                          : data["align"].toString() == "right"
                              ? Qt::AlignRight
                              : Qt::AlignHCenter;
        const auto pad=data["padding"].toDouble(8);
        p->drawText(box.adjusted(pad, pad, -pad, -pad),
                    align | Qt::AlignVCenter | Qt::TextWordWrap, text);
      }
    }
    p->restore();
    if(data.value("kind")=="group"&&!isSelected()){QPen pen(QColor("#6a879e"),1,Qt::DashLine);pen.setCosmetic(true);p->setPen(pen);p->setBrush(Qt::NoBrush);p->drawRect(box);}
    if (isSelected()) {
      QPen pen(QColor("#65a6ff"), 2);
      pen.setCosmetic(true);
      p->setPen(pen);
      p->setBrush(Qt::NoBrush);
      p->drawRect(box);
      if (!data["locked"].toBool())
        for (auto pt : {box.topLeft(), box.topRight(), box.bottomLeft(),
                        box.bottomRight()}) {
          p->setBrush(Qt::white);
          p->drawRect(QRectF(pt - QPointF(4, 4), QSizeF(8, 8)));
        }
    }
  }
  void paintPlugin(QPainter *p) {
    QString caption = canvas->pluginPlaceholder ? canvas->pluginPlaceholder(data) : QObject::tr("플러그인 필요");
    if (caption.isEmpty()) {
      p->save();
      p->setClipRect(box, Qt::IntersectClip);
      const bool painted = canvas->pluginPaint && canvas->pluginPaint(p, data, box);
      p->restore();
      if (painted) return;
      const auto ext = data.value("extension").toObject();
      caption = ext.value("type").toString() + " · " + (ext.value("ref").toString().isEmpty() ? QObject::tr("연결 문서 없음") : ext.value("ref").toString());
    }
    // Host-level fallback: `extension.props.posterAssetId` names a project image shown
    // as a still preview while the plugin's authoring module is unavailable. Other props
    // are plugin-defined and are deliberately not interpreted here.
    if (!image.isNull()) {
      QSizeF scaled = image.size();
      scaled.scale(box.size(), Qt::KeepAspectRatio);
      p->save();
      p->setClipRect(box, Qt::IntersectClip);
      p->drawPixmap(QRectF(QPointF((box.width() - scaled.width()) / 2, (box.height() - scaled.height()) / 2), scaled), image, QRectF(image.rect()));
      QPen posterPen(QColor("#e0a040"), 1, Qt::DashLine);
      posterPen.setCosmetic(true);
      p->setPen(posterPen);
      p->setBrush(Qt::NoBrush);
      p->drawRect(box.adjusted(1, 1, -1, -1));
      p->restore();
      return;
    }
    // A hatched box keeps missing/disabled plugin elements visible and movable
    // without pretending to render them; their data is never touched.
    p->fillRect(box, QColor(40, 44, 52, 200));
    p->fillRect(box, QBrush(QColor("#e0a040"), Qt::BDiagPattern));
    QPen pen(QColor("#e0a040"), 2, Qt::DashLine);
    pen.setCosmetic(true);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawRect(box.adjusted(1, 1, -1, -1));
    QFont font("Noto Sans KR");
    font.setPixelSize(16);
    font.setBold(true);
    p->setFont(font);
    const auto text = data.value("label").toString() + QChar(0x0a) + caption;
    const auto bounds = p->boundingRect(box.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, text).adjusted(-8, -6, 8, 6);
    p->fillRect(bounds.intersected(box), QColor(24, 26, 30, 230));
    p->setPen(QColor("#ffe2a8"));
    p->drawText(box.adjusted(12, 12, -12, -12), Qt::AlignCenter | Qt::TextWordWrap, text);
  }
  void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
    if (e->button() != Qt::LeftButton) {
      e->ignore();
      return;
    }
    if(e->modifiers()&(Qt::ControlModifier|Qt::ShiftModifier))setSelected(!isSelected());
    else if(!isSelected()){scene()->clearSelection();setSelected(true);}
    setFocus();
    origin = e->scenePos();
    before = QRectF(pos(), box.size());
    corner = -1;
    multipleDrag=scene()->selectedItems().size()>1;
    if(multipleDrag){canvas->startSelectionDrag();e->accept();return;}
    const QList<QPointF> corners = {box.topLeft(), box.topRight(),
                                    box.bottomLeft(), box.bottomRight()};
    for (int i = 0; i < corners.size(); i++)
      if (QLineF(e->pos(), corners[i]).length() < 12)
        corner = i;
    e->accept();
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override {
    if(!isSelected())return;
    if(multipleDrag){canvas->previewSelectionDrag((e->scenePos()-origin).toPoint());e->accept();return;}
    if (data["locked"].toBool())
      return;
    QPointF delta = e->scenePos() - origin;
    QRectF next = before;
    if (corner < 0)
      next.translate(delta);
    else {
      if (corner == 0)
        next.setTopLeft(before.topLeft() + delta);
      if (corner == 1)
        next.setTopRight(before.topRight() + delta);
      if (corner == 2)
        next.setBottomLeft(before.bottomLeft() + delta);
      if (corner == 3)
        next.setBottomRight(before.bottomRight() + delta);
      if (next.width() < 16 || next.height() < 16)
        return;
    }
    const int grid = canvas->snap ? 8 : 1;
    auto rounded = [grid](double v) { return std::round(v / grid) * grid; };
    const bool nested=data.value("_hasParent").toBool();
    next.setWidth(qBound(16., rounded(next.width()), nested?3840.:double(bounds.width())));
    next.setHeight(
        qBound(16., rounded(next.height()), nested?2160.:double(bounds.height())));
    next.moveLeft(
        qBound(nested?data.value("_parentX").toDouble()-3840.:0., rounded(next.x()), nested?data.value("_parentX").toDouble()+3840.:double(bounds.width()) - next.width()));
    next.moveTop(
        qBound(nested?data.value("_parentY").toDouble()-2160.:0., rounded(next.y()), nested?data.value("_parentY").toDouble()+2160.:double(bounds.height()) - next.height()));
    prepareGeometryChange();
    box.setSize(next.size());
    setPos(next.topLeft());
    update();
    e->accept();
  }
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *e) override {
    if(multipleDrag){canvas->finishSelectionDrag();multipleDrag=false;e->accept();return;}
    const QRect rectangle = QRectF(pos(), box.size()).toRect();
    if (rectangle != before.toRect())
      emit canvas->geometryEdited(data["id"].toString(), authoredRectangle(rectangle));
    e->accept();
  }
};

ScreenCanvas::ScreenCanvas(QWidget *parent) : QGraphicsView(parent) {
  setObjectName("screenCanvas");
  setScene(&scene);
  setBackgroundBrush(QColor("#191d22"));
  setRenderHint(QPainter::Antialiasing);
  setRenderHint(QPainter::SmoothPixmapTransform);
  setViewportUpdateMode(BoundingRectViewportUpdate);
  setFocusPolicy(Qt::StrongFocus);
  setDragMode(RubberBandDrag);
  setRubberBandSelectionMode(Qt::ContainsItemShape);
  connect(&scene, &QGraphicsScene::selectionChanged, this, [this] {
    if (!refreshing) {
      const auto ids=selectedIds();
      emit selectionsChanged(ids);
      if(ids.size()<2)emit selectionChanged(ids.value(0));
    }
  });
}
void ScreenCanvas::setScreen(const QJsonObject &project,
                             const QJsonObject &screen,
                             const QJsonArray &assets, const QString &selected,
                             bool fit) {
  cachedProject=project;cachedScreen=screen;cachedAssets=assets;runtimeActive=false;runtimeUi={};
  refreshing = true;
  sourceElements=screen.value("elements").toArray();dragStart.clear();dragDelta={};
  scene.clear();
  auto visual = project["visual"].toObject();
  canvasSize = {project["_previewWidth"].toInt(visual["width"].toInt(1280)), project["_previewHeight"].toInt(visual["height"].toInt(720))};
  scene.setSceneRect(
      QRectF(QPointF(-24, -24),
             QSizeF(canvasSize.width() + 48, canvasSize.height() + 48)));
  scene
      .addRect(QRectF(QPointF(), canvasSize), QPen(QColor("#555c66")),
               QBrush(QColor("#dce9df")))
      ->setZValue(-1);
  QMap<QString, QString> paths;
  QMap<QString,QImage> surfaceImages;
  for (auto a : assets) {
    auto v = a.toObject();
    paths[v["id"].toString()] = v["absolutePath"].toString();
  }
  QString background =
      visual["defaultScene"].toObject()["backgroundAssetId"].toString();
  for (auto loc : project["gameplay"].toObject()["locations"].toArray())
    if (loc.toObject()["screenId"] == screen["id"])
      background = loc.toObject()["presentation"]
                       .toObject()["backgroundAssetId"]
                       .toString(background);
  int index = 0;
  for (auto v : canvasElements(visual,screen["elements"].toArray(),canvasSize)) {
    auto e = resolvedStyle(visual,v.toObject());
    const auto fontPath=paths.value(e["fontAssetId"].toString(),project["_defaultFontPath"].toString());
    const auto family=projectFontFamily(fontPath);if(!family.isEmpty())e["_fontFamily"]=family;
    if(e["kind"]=="schedule"){e["_calendarProject"]=QJsonObject{{"startYear",project["startYear"]},{"startMonth",project["startMonth"]},{"periodsPerMonth",project["periodsPerMonth"]},{"calendar",project["calendar"]}};e["_year"]=project["startYear"];e["_month"]=project["startMonth"];e["_rosterPreview"]=initialRosterPreview(project);}
    if(e["kind"]=="schedule"&&e["scheduleLayout"]=="activity-board"){
      e["_boardActivities"]=project.value("_previewActivities");QJsonObject art;for(auto it=paths.begin();it!=paths.end();++it)art[it.key()]=it.value();e["_boardAssets"]=art;
      for(const auto& row:visual.value("styles").toArray())if(row.toObject().value("id")==e.value("controlStyleId"))e["_boardControlStyle"]=row.toObject();
    }
    if(e["kind"]=="choices")e["_choicePreview"]=initialChoicePreview(project.value("_previewNarratives").toArray());
    if (e["hidden"].toBool() || project["_previewHiddenIds"].toArray().contains(e["id"]))
      continue;
    if(e["kind"]=="input"||(e["kind"]=="text"&&e["binding"]=="user-field")){
      QString value;for(auto field:project.value("userFields").toArray())if(field.toObject().value("id")==e.value("userFieldId"))value=field.toObject().value("initial").toString();
      e["text"]=e["kind"]=="input"?value:e.value("valueFormat").toString("{value}").replace("{value}",value);
    }
    if (e["kind"] == "text") {
      const auto binding = e["binding"].toString();
      if (binding == "calendar")
        e["text"] = StudioCalendar::label(project,0);
      else if (binding == "title")
        e["text"] = project["title"];
      else if (binding == "context-name") e["text"] = contextNameForPreview(project,e);
      else if (binding == "narrative-actor") e["text"] = tr("사건 대상 · 플레이 중 결정");
      else if (binding == "calendar-year") e["text"] = QString::number(project["startYear"].toInt(1));
      else if (binding == "calendar-month") e["text"] = project.contains("calendar")?QString::fromStdString(StudioCalendar::spec(project).month)+" "+QString::number(project["startMonth"].toInt(1)):QString::number(project["startMonth"].toInt(1)) + QObject::tr("월");
      else if (binding == "calendar-day") e["text"] = "1";
      else if (binding == "calendar-weekday") e["text"] = project.contains("calendar")?QString::fromStdString(StudioCalendar::spec(project).weekdays[StudioCalendar::spec(project).startWeekday]):QObject::tr("요일");
      else if (binding == "schedule-prompt") e["text"] = QObject::tr("첫 번째 일정을 결정해 주세요.");
      else if (binding == "description")
        e["text"] = project["description"];
      else if (binding == "location")
        e["text"] = screen["label"];
      else if(binding=="data-value") {
        const auto target=e.value("dataBinding").toObject();const auto kind=target.value("kind").toString();QJsonObject field;
        if(kind=="attribute")field=project.value("_previewAttributes").toObject().value(target.value("attributeId").toString()).toObject();
        if(kind=="relationship")field=project.value("_previewRelationships").toObject().value(target.value("characterId").toString()).toObject().value(target.value("relationshipId").toString()).toObject();
        if(kind=="inventory")for(const auto& row:project.value("gameplay").toObject().value("items").toArray()){const auto item=row.toObject();if(item.value("id")==target.value("itemId"))field=QJsonObject{{"initial",item.value("initialQuantity").toInt()},{"label",item.value("name")}};}
        if(target.contains("owner"))for(const auto& row:project.value("roster").toObject().value("members").toArray()){const auto member=row.toObject();if(member.value("characterId")!=target.value("owner").toObject().value("characterId"))continue;
          if(kind=="attribute")field["initial"]=member.value("initialAttributes").toObject().value(target.value("attributeId").toString()).toInt(field.value("initial").toInt());
          if(kind=="inventory")field["initial"]=member.value("initialInventory").toObject().value(target.value("itemId").toString()).toInt();
          if(kind=="relationship")field["initial"]=member.value("initialRelationships").toObject().value(target.value("characterId").toString()).toObject().value(target.value("relationshipId").toString()).toInt(field.value("initial").toInt());
        }
        e["text"]=e.value("valueFormat").toString("{label}: {value}").replace("{label}",field.value("label").toString()).replace("{value}",field.isEmpty()?QString::fromUtf8("—"):QString::number(field.value("initial").toDouble()));
      }
      else if (binding == "attribute-value" ||
               binding == "relationship-value" || binding == "item-count") {
        QString value = e["valueFormat"].toString("{label}: {value}");
        auto attribute = project["_previewAttributes"]
                             .toObject()[e["attributeId"].toString()]
                             .toObject();
        if(binding=="relationship-value")attribute=project.value("_previewRelationships").toObject().value(e.value("characterId").toString()).toObject().value(e.value("relationshipId").toString()).toObject();
        value.replace("{delta}","0");
        value.replace("{value}",
                      attribute.isEmpty()
                          ? QString::fromUtf8("…")
                          : QString::number(attribute["initial"].toDouble()));
        value.replace(
            "{label}",
            attribute["label"].toString(e["attributeId"].toString(
                e["itemId"].toString(e["relationshipId"].toString()))));
        e["text"] = value;
      }
    }
    QString asset = e["imageSource"] == "background"
                        ? background
                        : e["imageAssetId"].toString();
    if(e["kind"]=="plugin")asset=e["extension"].toObject()["props"].toObject()["posterAssetId"].toString();
    if (e["imageSource"].toString().startsWith("portrait-"))
      asset = project["_previewPortraits"]
                  .toObject()[e["imageSource"].toString().mid(9)]
                  .toString();
    QString file = paths.value(asset);
    if (!file.isEmpty() && !imageCache.contains(file))
      imageCache[file] = QPixmap(file);
    auto item = new VisualItem(e, imageCache.value(file), this, canvasSize);
    const auto surfaceFile=paths.value(e.value("surface").toObject().value("imageAssetId").toString());
    if(!surfaceFile.isEmpty()){if(!surfaceImages.contains(surfaceFile))surfaceImages[surfaceFile]=loadSurfaceImage(surfaceFile);item->surfaceImage=surfaceImages.value(surfaceFile);}
    if(e["_componentChild"].toBool()||project["_responsivePreview"].toBool()) {
      item->setFlag(QGraphicsItem::ItemIsSelectable,false);
      item->setFlag(QGraphicsItem::ItemIsFocusable,false);
      item->setAcceptedMouseButtons(Qt::NoButton);
    }
    scene.addItem(item);
    item->setZValue(index++);
    item->setSelected(e["id"].toString() == selected);
  }
  refreshing = false;
  if (fit) {
    autoFit = true;
    fitCanvas();
  }
}
void ScreenCanvas::clearRuntimePreview(){
  if(!runtimeActive)return;
  const auto ids=selectedIds();setScreen(cachedProject,cachedScreen,cachedAssets,ids.value(0));selectMany(ids);
}
void ScreenCanvas::setRuntimePreview(const QJsonObject& ui,const QImage& pixels){
  if(pixels.isNull()||ui.value("screenId")!=cachedScreen.value("id"))return;
  clearRuntimePreview();runtimeActive=true;runtimeUi=ui;
  QMap<QString,QJsonObject> nodes;for(auto value:ui.value("nodes").toArray()){auto n=value.toObject();nodes[n.value("id").toString()]=n;}
  const auto presentation=ui.value("presentation").toObject();
  const double sx=canvasSize.width()/qMax(1.,presentation.value("width").toDouble(canvasSize.width()));
  const double sy=canvasSize.height()/qMax(1.,presentation.value("height").toDouble(canvasSize.height()));
  for(auto item:scene.items()){
    auto visual=dynamic_cast<VisualItem*>(item);if(!visual){item->hide();continue;}
    auto node=nodes.value(visual->data.value("id").toString());visual->runtimeLayer=true;
    visual->runtimeVisible=node.value("visible").toBool()&&node.value("rendered").toBool()&&!node.value("outsideViewport").toBool();
    if(visual->runtimeVisible){
      visual->setRuntimeGeometry(QRectF((node.value("x").toDouble()-presentation.value("x").toDouble())*sx,(node.value("y").toDouble()-presentation.value("y").toDouble())*sy,node.value("width").toDouble()*sx,node.value("height").toDouble()*sy));
    }
    visual->runtimeBase=QRectF(visual->pos(),visual->box.size());
    visual->setToolTip(ui.value("texts").toObject().value(visual->data.value("id").toString()).toString());visual->update();
  }
  auto backdrop=scene.addPixmap(QPixmap::fromImage(pixels));backdrop->setZValue(-2);backdrop->setAcceptedMouseButtons(Qt::NoButton);
  QTransform scale;scale.scale(double(canvasSize.width())/pixels.width(),double(canvasSize.height())/pixels.height());backdrop->setTransform(scale);
}
void ScreenCanvas::select(const QString &id) {
  selectMany(id.isEmpty()?QStringList{}:QStringList{id});
}
void ScreenCanvas::selectMany(const QStringList &ids) {
  refreshing = true;
  for (auto item : scene.items())
    if (item->flags() & QGraphicsItem::ItemIsSelectable)
      item->setSelected(ids.contains(item->data(0).toString()));
  refreshing = false;
}
QStringList ScreenCanvas::selectedIds() const {
  QStringList ids;for(auto item:scene.items(Qt::AscendingOrder))if(item->isSelected()&&(item->flags()&QGraphicsItem::ItemIsSelectable))ids.append(item->data(0).toString());return ids;
}
void ScreenCanvas::startSelectionDrag(){
  dragStart.clear();dragDelta={};auto ids=selectedIds();QSet<QString> affected(ids.begin(),ids.end());bool added=true;
  while(added){added=false;for(auto v:sourceElements){auto e=v.toObject();auto id=e["id"].toString();if(affected.contains(e["parentId"].toString())&&!affected.contains(id)){affected.insert(id);added=true;}}}
  for(auto item:scene.items()){const auto id=item->data(0).toString();bool include=affected.contains(id);for(const auto &root:affected)include=include||id.startsWith(root+"__");if(include)dragStart[item]=item->pos();}
}
void ScreenCanvas::previewSelectionDrag(QPoint delta){
  const int grid=snap?8:1;dragDelta={qRound(double(delta.x())/grid)*grid,qRound(double(delta.y())/grid)*grid};QJsonArray next;QString error;
  const bool valid=editSelection(sourceElements,selectedIds(),canvasSize,{{"mode","move"},{"dx",dragDelta.x()},{"dy",dragDelta.y()}},next,error);
  for(auto it=dragStart.begin();it!=dragStart.end();++it)it.key()->setPos(it.value()+(valid?dragDelta:QPoint()));
}
void ScreenCanvas::finishSelectionDrag(){const auto delta=dragDelta;for(auto it=dragStart.begin();it!=dragStart.end();++it)it.key()->setPos(it.value());dragStart.clear();dragDelta={};if(!delta.isNull())emit selectionMoved(delta);}
void ScreenCanvas::fitCanvas() {
  autoFit = true;
  fitInView(scene.sceneRect(), Qt::KeepAspectRatio);
}
void ScreenCanvas::resizeEvent(QResizeEvent *e) {
  QGraphicsView::resizeEvent(e);
  if (autoFit)
    fitCanvas();
}
void ScreenCanvas::wheelEvent(QWheelEvent *e) {
  if (e->modifiers() & Qt::ControlModifier) {
    autoFit = false;
    setTransformationAnchor(AnchorUnderMouse);
    double s = e->angleDelta().y() > 0 ? 1.12 : 1 / 1.12;
    if (transform().m11() * s > .12 && transform().m11() * s < 5)
      scale(s, s);
    e->accept();
  } else
    QGraphicsView::wheelEvent(e);
}
void ScreenCanvas::keyPressEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
    panning = true;
    setDragMode(ScrollHandDrag);
    setInteractive(false);
    e->accept();
    return;
  }
  if (e->key() == Qt::Key_F) {
    fitCanvas();
    return;
  }
  if (e->key() == Qt::Key_Escape) {
    scene.clearSelection();
    return;
  }
  if (e->key() == Qt::Key_Delete) {
    emit deleteSelected();
    return;
  }
  if (e->key() == Qt::Key_D && (e->modifiers() & Qt::ControlModifier)) {
    emit duplicateSelected();
    return;
  }
  auto selected = scene.selectedItems();
  if(selected.size()>1){QPoint delta;const int step=e->modifiers()&Qt::ShiftModifier?10:1;if(e->key()==Qt::Key_Left)delta={-step,0};if(e->key()==Qt::Key_Right)delta={step,0};if(e->key()==Qt::Key_Up)delta={0,-step};if(e->key()==Qt::Key_Down)delta={0,step};if(!delta.isNull()){emit selectionMoved(delta);return;}}
  if (!selected.isEmpty()) {
    auto item = dynamic_cast<VisualItem *>(selected.first());
    int step = e->modifiers() & Qt::ShiftModifier ? 10 : 1;
    QPoint delta;
    if (e->key() == Qt::Key_Left)
      delta = {-step, 0};
    if (e->key() == Qt::Key_Right)
      delta = {step, 0};
    if (e->key() == Qt::Key_Up)
      delta = {0, -step};
    if (e->key() == Qt::Key_Down)
      delta = {0, step};
    if (item && !item->data["locked"].toBool() && !delta.isNull()) {
      auto rect =
          QRectF(item->pos(), item->box.size()).toRect().translated(delta);
      const bool nested=item->data.value("_hasParent").toBool();
      const QPoint parent(qRound(item->data.value("_parentX").toDouble()),qRound(item->data.value("_parentY").toDouble()));
      if(nested){rect.translate(-parent);rect.moveLeft(qBound(-3840,rect.x(),3840));rect.moveTop(qBound(-2160,rect.y(),2160));}
      else {rect.moveLeft(qBound(0, rect.x(), canvasSize.width() - rect.width()));rect.moveTop(qBound(0, rect.y(), canvasSize.height() - rect.height()));}
      if(item->runtimeLayer)rect=item->authoredRectangle(QRectF(item->pos(),item->box.size()).translated(delta));
      emit geometryEdited(item->data["id"].toString(), rect);
      return;
    }
  }
  QGraphicsView::keyPressEvent(e);
}
void ScreenCanvas::keyReleaseEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
    panning = false;
    setDragMode(RubberBandDrag);
    setInteractive(true);
    e->accept();
    return;
  }
  QGraphicsView::keyReleaseEvent(e);
}
