#include "canvas.hpp"
#include "design.hpp"
#include "components.hpp"
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
  ScreenCanvas *canvas;
  QSize bounds;
  QRectF box;
  QPointF origin;
  QRectF before;
  int corner = -1;
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
    if(data.value("kind")=="group"&&!isSelected()) {p.addRect(box.adjusted(-3,-3,3,3));p.addRect(box.adjusted(3,3,-3,-3));p.setFillRule(Qt::OddEvenFill);}
    else p.addRect(box);
    if(data.contains("_clipRect")){QPainterPath clip;clip.addRect(canvasClipRect(data.value("_clipRect").toObject()).translated(-pos()));return p.intersected(clip);}
    return p;
  }
  void paint(QPainter *p, const QStyleOptionGraphicsItem *,
             QWidget *) override {
    p->setRenderHint(QPainter::Antialiasing);
    p->save();
    if(data.contains("_clipRect"))p->setClipRect(canvasClipRect(data.value("_clipRect").toObject()).translated(-pos()));
    p->setOpacity(data["opacity"].toDouble(1));
    const auto kind = data["kind"].toString();
    if (kind == "image" && !image.isNull()) {
      const bool contain = data["fit"].toString() == "contain";
      QSizeF scaled = image.size();
      scaled.scale(box.size(), contain ? Qt::KeepAspectRatio
                                       : Qt::KeepAspectRatioByExpanding);
      QRectF target(QPointF((box.width() - scaled.width()) / 2,
                            (box.height() - scaled.height()) / 2),
                    scaled);
      p->setClipRect(box,Qt::IntersectClip);
      p->drawPixmap(target, image, QRectF(image.rect()));
    } else if (kind == "meter") {
      p->fillRect(box,rgba(data.value("fill").toString(),QColor("#16242e")));
      const double maximum=qMax(0.0001,data.value("maxValue").toDouble(100));
      const double ratio=qBound(0.0,data.value("value").toDouble()/maximum,1.0);
      p->fillRect(QRectF(0,0,box.width()*ratio,box.height()),rgba(data.value("color").toString(),QColor("#68c6a0")));
      p->setPen(rgba(data.value("color").toString(),Qt::white));
      p->drawText(box,Qt::AlignCenter,data.value("text").toString());
    } else if (kind == "schedule" && data["scheduleLayout"] == "calendar") {
      p->fillRect(box, QColor("#080706")); p->setPen(QColor("#cba99d"));
      QFont font(data["_fontFamily"].toString("Noto Sans KR"));font.setPixelSize(data["fontSize"].toInt(24));p->setFont(font);
      QDate date(data["_year"].toInt(1209), data["_month"].toInt(4), 1);
      p->drawText(QRectF(0,0,box.width(),36),Qt::AlignCenter,QString::fromUtf8("%1년 %2월").arg(date.year()).arg(date.month()));
      const QStringList days={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
      const qreal cw=box.width()/7,ch=(box.height()-80)/6;
      for(int i=0;i<7;i++)p->drawText(QRectF(i*cw,40,cw,32),Qt::AlignCenter,days[i]);
      for(int day=1;day<=date.daysInMonth();day++){int cell=date.dayOfWeek()%7+day-1;QRectF r((cell%7)*cw,80+(cell/7)*ch,cw,ch);p->drawRect(r);p->drawText(r.adjusted(3,2,-3,-2),Qt::AlignLeft|Qt::AlignTop,QString::number(day));}
    } else if (kind == "schedule" && data["scheduleLayout"] == "menu") {
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
      if (kind != "panel" && kind != "group") {
        QString text = data["text"].toString();
        if (text.isEmpty() && !data.contains("text"))
          text = data["label"].toString();
        if (text.isEmpty() && kind == "text" && !data.contains("text"))
          text = data["binding"].toString();
        if (text.isEmpty() && !data.contains("text")) {
          static const QMap<QString, QString> labels = {
              {"locations", "이동할 장소"},
              {"shop", "상품 목록 · 실제 내용은 플레이에서 확인"},
              {"sell", "판매 목록 · 수량 / 확인창"},
              {"services", "병원 · 비용 / 치료 효과"},
              {"actions", "공통 행동 · 조건 / 비용 / 효과"},
              {"policies", "정책 그룹 · 독립 선택 / 실행 주기"},
              {"diets", "지속 식단 · 일정별 비용 / 효과"},
              {"inventory", "소지품"},
              {"stats", "능력 · 자원"},
              {"image", "이미지 없음"},
              {"schedule", "일정 선택"},
              {"dialogue", "대사"}};
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
  void mousePressEvent(QGraphicsSceneMouseEvent *e) override {
    if (e->button() != Qt::LeftButton) {
      e->ignore();
      return;
    }
    scene()->clearSelection();
    setSelected(true);
    setFocus();
    origin = e->scenePos();
    before = QRectF(pos(), box.size());
    corner = -1;
    const QList<QPointF> corners = {box.topLeft(), box.topRight(),
                                    box.bottomLeft(), box.bottomRight()};
    for (int i = 0; i < corners.size(); i++)
      if (QLineF(e->pos(), corners[i]).length() < 12)
        corner = i;
    e->accept();
  }
  void mouseMoveEvent(QGraphicsSceneMouseEvent *e) override {
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
    const QRect rectangle = QRectF(pos(), box.size()).toRect();
    if (rectangle != before.toRect())
      emit canvas->geometryEdited(data["id"].toString(), rectangle.translated(-qRound(data.value("_parentX").toDouble()),-qRound(data.value("_parentY").toDouble())));
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
  connect(&scene, &QGraphicsScene::selectionChanged, this, [this] {
    if (!refreshing) {
      auto items = scene.selectedItems();
      emit selectionChanged(
          items.isEmpty() ? QString() : items.first()->data(0).toString());
    }
  });
}
void ScreenCanvas::setScreen(const QJsonObject &project,
                             const QJsonObject &screen,
                             const QJsonArray &assets, const QString &selected,
                             bool fit) {
  refreshing = true;
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
    if(e["kind"]=="schedule"){e["_year"]=project["startYear"];e["_month"]=project["startMonth"];}
    if (e["hidden"].toBool())
      continue;
    if(e["kind"]=="input"||(e["kind"]=="text"&&e["binding"]=="user-field")){
      QString value;for(auto field:project.value("userFields").toArray())if(field.toObject().value("id")==e.value("userFieldId"))value=field.toObject().value("initial").toString();
      e["text"]=e["kind"]=="input"?value:e.value("valueFormat").toString("{value}").replace("{value}",value);
    }
    if (e["kind"] == "text") {
      const auto binding = e["binding"].toString();
      if (binding == "calendar")
        e["text"] = QString::fromUtf8("%1년 %2월 · 1번째 일정")
                        .arg(project["startYear"].toInt(1))
                        .arg(project["startMonth"].toInt(1));
      else if (binding == "title")
        e["text"] = project["title"];
      else if (binding == "calendar-year") e["text"] = QString::number(project["startYear"].toInt(1));
      else if (binding == "calendar-month") e["text"] = QString::number(project["startMonth"].toInt(1)) + QString::fromUtf8("월");
      else if (binding == "calendar-day") e["text"] = "1";
      else if (binding == "calendar-weekday") e["text"] = QString::fromUtf8("요일");
      else if (binding == "schedule-prompt") e["text"] = QString::fromUtf8("첫 번째 일정을 결정해 주세요.");
      else if (binding == "description")
        e["text"] = project["description"];
      else if (binding == "location")
        e["text"] = screen["label"];
      else if (binding == "attribute-value" ||
               binding == "relationship-value" || binding == "item-count") {
        QString value = e["valueFormat"].toString("{label}: {value}");
        auto attribute = project["_previewAttributes"]
                             .toObject()[e["attributeId"].toString()]
                             .toObject();
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
    if (e["imageSource"].toString().startsWith("portrait-"))
      asset = project["_previewPortraits"]
                  .toObject()[e["imageSource"].toString().mid(9)]
                  .toString();
    QString file = paths.value(asset);
    if (!file.isEmpty() && !imageCache.contains(file))
      imageCache[file] = QPixmap(file);
    auto item = new VisualItem(e, imageCache.value(file), this, canvasSize);
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
void ScreenCanvas::select(const QString &id) {
  refreshing = true;
  for (auto item : scene.items())
    if (item->flags() & QGraphicsItem::ItemIsSelectable)
      item->setSelected(item->data(0).toString() == id);
  refreshing = false;
}
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
      emit geometryEdited(item->data["id"].toString(), rect);
      return;
    }
  }
  QGraphicsView::keyPressEvent(e);
}
void ScreenCanvas::keyReleaseEvent(QKeyEvent *e) {
  if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
    panning = false;
    setDragMode(NoDrag);
    setInteractive(true);
    e->accept();
    return;
  }
  QGraphicsView::keyReleaseEvent(e);
}
