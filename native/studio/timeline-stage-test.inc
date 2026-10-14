#include "timeline-stage.hpp"
#include <QDoubleSpinBox>
#include <QTemporaryDir>

void StudioTests::timelineStageAuthoring(){
  QTemporaryDir temp;QVERIFY(temp.isValid());
  QImage red(4,4,QImage::Format_ARGB32);red.fill(Qt::red);QVERIFY(red.save(temp.path()+"/red.png"));
  QImage blue(4,4,QImage::Format_ARGB32);blue.fill(Qt::blue);QVERIFY(blue.save(temp.path()+"/blue.png"));
  const QJsonArray assets{QJsonObject{{"id","red"},{"kind","image"},{"absolutePath",temp.path()+"/red.png"}},QJsonObject{{"id","blue"},{"kind","image"},{"absolutePath",temp.path()+"/blue.png"}}};
  QJsonObject animated{{"id","actor"},{"x",1.123456},{"y",1},{"width",2},{"height",2},{"frames",QJsonArray{QJsonObject{{"imageAssetId","red"},{"durationMs",100}},QJsonObject{{"imageAssetId","blue"},{"durationMs",200}}}}};
  QCOMPARE(timelineLayerFrame(animated,99),QString("red"));QCOMPARE(timelineLayerFrame(animated,100),QString("blue"));QCOMPARE(timelineLayerFrame(animated,300),QString("blue"));animated["loop"]=true;QCOMPARE(timelineLayerFrame(animated,300),QString("red"));animated.remove("loop");
  QJsonObject background{{"id","background"},{"x",0},{"y",0},{"width",4},{"height",4},{"frames",QJsonArray{QJsonObject{{"imageAssetId","red"},{"durationMs",1}}}}};
  QJsonObject stage{{"width",4},{"height",4},{"layers",QJsonArray{background,animated}}};
  QMap<QString,QPixmap> images{{"red",QPixmap::fromImage(red)},{"blue",QPixmap::fromImage(blue)}};
  QImage render(40,40,QImage::Format_ARGB32);render.fill(Qt::black);{QPainter p(&render);drawTimelineStage(p,render.rect(),stage,images,100);}
  QCOMPARE(render.pixelColor(2,2),QColor(Qt::red));QCOMPARE(render.pixelColor(20,20),QColor(Qt::blue));
  auto baseline=stage;
  QTimer::singleShot(0,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);d->findChild<QListWidget*>("stageLayers")->setCurrentRow(1);d->findChild<QDoubleSpinBox*>("stageLayerx")->setValue(9);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel)->click();});
  QVERIFY(!editTimelineStage(nullptr,stage,assets,900));QCOMPARE(stage,baseline);
  // Valid LLM-authored values more precise than the displayed controls survive
  // opening/saving and unrelated edits, including fractional stage sizes.
  stage["width"]=0.5;stage["height"]=0.75;baseline=stage;
  QTimer::singleShot(0,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);d->findChild<QListWidget*>("stageLayers")->setCurrentRow(1);d->findChild<QDoubleSpinBox*>("stageLayery")->setValue(3);d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  QVERIFY(editTimelineStage(nullptr,stage,assets,900));QCOMPARE(stage.value("width"),baseline.value("width"));QCOMPARE(stage.value("height"),baseline.value("height"));auto result=stage.value("layers").toArray()[1].toObject();QCOMPARE(result.value("x"),animated.value("x"));QCOMPARE(result.value("y").toDouble(),3.0);QVERIFY(!result.contains("loop"));
  QTimer::singleShot(0,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);QTimer::singleShot(5000,d,&QDialog::reject);auto list=d->findChild<QListWidget*>("stageLayers");list->setCurrentRow(1);d->findChild<QPushButton*>("stageLayerUp")->click();QCOMPARE(list->item(0)->text(),QString("actor"));d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  QVERIFY(editTimelineStage(nullptr,stage,assets,900));QCOMPARE(stage.value("layers").toArray()[0].toObject().value("id").toString(),QString("actor"));
  QTimer::singleShot(0,[&]{auto d=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(d);d->findChild<QPushButton*>("stageClear")->click();});
  QVERIFY(editTimelineStage(nullptr,stage,assets,900));QVERIFY(stage.isEmpty());
}
