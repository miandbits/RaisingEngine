#include "studio.hpp"
#include "selection-edit.hpp"
#include "components.hpp"
#include <QCheckBox>
#include <QDialog>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDockWidget>
#include <QDialogButtonBox>
#include <QFile>
#include <QGraphicsItem>
#include <QJsonDocument>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QTreeWidgetItemIterator>

class SelectionTests:public QObject{
  Q_OBJECT
private slots:
  void atomicGeometry(){
    QJsonArray es{QJsonObject{{"id","a"},{"kind","button"},{"x",20},{"y",10},{"width",40},{"height",20},{"actionId","kept"},{"anchorX","right"}},QJsonObject{{"id","b"},{"kind","text"},{"x",110},{"y",40},{"width",20},{"height",20}},QJsonObject{{"id","c"},{"kind","image"},{"x",230},{"y",90},{"width",30},{"height",20}}};QJsonArray out;QString error;
    QVERIFY(editSelection(es,{"a","b","c"},{400,300},{{"mode","distribute"},{"axis","x"}},out,error));QCOMPARE(out[1].toObject()["x"].toInt(),135);QCOMPARE(out[0],es[0]);QCOMPARE(out[2],es[2]);
    QVERIFY(editSelection(es,{"a","b"},{400,300},{{"mode","align"},{"axis","y"},{"edge","end"}},out,error));QCOMPARE(out[0].toObject()["y"].toInt(),40);QCOMPARE(out[0].toObject()["actionId"].toString(),QString("kept"));QCOMPARE(out[0].toObject()["anchorX"].toString(),QString("right"));
    QVERIFY(editSelection(es,{"a","b"},{400,300},{{"mode","align"},{"axis","x"},{"edge","end"},{"patch",QJsonObject{{"width",32}}}},out,error));QCOMPARE(out[0].toObject()["x"].toInt(),110);QCOMPARE(out[1].toObject()["x"].toInt(),110);
    auto locked=es[1].toObject();locked["locked"]=true;es[1]=locked;QVERIFY(!editSelection(es,{"a","b"},{400,300},{{"mode","move"},{"dx",3}},out,error));QCOMPARE(out,es);
    QVERIFY(!editSelection(es,{"a"},{400,300},{{"mode","move"},{"dx",500}},out,error));QCOMPARE(out,es);
    QVERIFY(editSelection(es,{"a","b"},{400,300},{{"mode","properties"},{"patch",QJsonObject{{"opacity",.8}}}},out,error));QCOMPARE(out[0].toObject()["opacity"].toDouble(),.8);QCOMPARE(out[1].toObject()["locked"].toBool(),true);
  }
  void parentsAndLocks(){
    QJsonArray es{QJsonObject{{"id","g"},{"kind","group"},{"x",40},{"y",40},{"width",100},{"height",100}},QJsonObject{{"id","c"},{"kind","text"},{"parentId","g"},{"x",10},{"y",10},{"width",40},{"height",20}}};QJsonArray out;QString error;
    QVERIFY(editSelection(es,{"g","c"},{400,300},{{"mode","move"},{"dx",5}},out,error));QCOMPARE(out[0].toObject()["x"].toInt(),45);QCOMPARE(out[1],es[1]);
    auto g=es[0].toObject();g["locked"]=true;es[0]=g;QVERIFY(!editSelection(es,{"c"},{400,300},{{"mode","properties"},{"patch",QJsonObject{{"locked",false}}}},out,error));QCOMPARE(out,es);
    g.remove("locked");g["flowLayout"]=QJsonObject{{"direction","column"}};es[0]=g;QVERIFY(!editSelection(es,{"c"},{400,300},{{"mode","move"},{"dx",5}},out,error));QCOMPARE(out,es);
  }
  void canvasMultiSelection(){
    ScreenCanvas canvas;canvas.resize(640,480);canvas.show();QJsonObject p{{"visual",QJsonObject{{"width",400},{"height",300}}}};QJsonArray es{QJsonObject{{"id","a"},{"kind","button"},{"x",30},{"y",30},{"width",100},{"height",50}},QJsonObject{{"id","b"},{"kind","button"},{"x",200},{"y",30},{"width",100},{"height",50}}};canvas.setScreen(p,{{"elements",es}},{},{},true);QTest::qWait(20);
    QTest::mouseClick(canvas.viewport(),Qt::LeftButton,Qt::NoModifier,canvas.mapFromScene(QPointF(75,55)));QTest::mouseClick(canvas.viewport(),Qt::LeftButton,Qt::ControlModifier,canvas.mapFromScene(QPointF(245,55)));QCOMPARE(canvas.selectedIds(),QStringList({"a","b"}));
    QSignalSpy move(&canvas,&ScreenCanvas::selectionMoved);QTest::keyClick(&canvas,Qt::Key_Right,Qt::ShiftModifier);QCOMPARE(move.count(),1);QCOMPARE(move[0][0].toPoint(),QPoint(10,0));
    canvas.startSelectionDrag();canvas.previewSelectionDrag({16,8});canvas.finishSelectionDrag();QCOMPARE(move.count(),2);QCOMPARE(move[1][0].toPoint(),QPoint(16,8));
    QTest::mouseClick(canvas.viewport(),Qt::LeftButton,Qt::ControlModifier,canvas.mapFromScene(QPointF(245,55)));QCOMPARE(canvas.selectedIds(),QStringList({"a"}));
  }
  void editorJourney(){
    const auto file=qEnvironmentVariable("RAISING_MULTI_PLAN");if(file.isEmpty())QSKIP("Set RAISING_MULTI_PLAN to an isolated authoring plan");QFile input(file);QVERIFY(input.open(QIODevice::ReadOnly));const auto plan=QJsonDocument::fromJson(input.readAll()).object();
    QTimer watchdog;watchdog.setInterval(30000);connect(&watchdog,&QTimer::timeout,this,[]{auto modal=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(modal){qWarning()<<"Timed out modal"<<modal->objectName()<<modal->windowTitle();modal->reject();}});watchdog.start();
    StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()));w.show();w.openProject(plan["root"].toString());QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().empty(),20000);
    auto tree=w.findChild<QTreeWidget*>("hierarchy");auto canvas=w.findChild<ScreenCanvas*>("screenCanvas");QVERIFY(tree&&canvas);QCOMPARE(tree->selectionMode(),QAbstractItemView::ExtendedSelection);
    auto open=[&](QString id){auto list=w.findChild<QListWidget*>("projectList");QListWidgetItem *target=nullptr;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)==id)target=list->item(i);QVERIFY(target);QMetaObject::invokeMethod(list,"itemActivated",Q_ARG(QListWidgetItem*,target));};
    auto choose=[&](QStringList ids){tree->clearSelection();for(auto id:ids){QTreeWidgetItem *item=nullptr;for(QTreeWidgetItemIterator it(tree);*it;++it)if((*it)->data(0,Qt::UserRole)==id)item=*it;QVERIFY(item);item->setSelected(true);}QCOMPARE(w.selectedIds().size(),ids.size());QCOMPARE(canvas->selectedIds().size(),ids.size());};
    auto fill=[&](QDialog *dialog,QJsonObject edit){auto op=dialog->findChild<QComboBox*>("batchOperation");auto mode=edit["operation"].toString("properties");op->setCurrentIndex(op->findData(mode));dialog->findChild<QSpinBox*>("batchDx")->setValue(edit["dx"].toInt());dialog->findChild<QSpinBox*>("batchDy")->setValue(edit["dy"].toInt());auto patch=edit["patch"].toObject();for(auto it=patch.begin();it!=patch.end();++it){auto use=dialog->findChild<QCheckBox*>("batchUse-"+it.key());QVERIFY(use);use->setChecked(true);if(auto n=dialog->findChild<QSpinBox*>("batchValue-"+it.key()))n->setValue(it.value().toInt());else if(auto b=dialog->findChild<QCheckBox*>("batchValue-"+it.key()))b->setChecked(it.value().toBool());}if(!plan["evidence"].toString().isEmpty())QVERIFY(dialog->grab().save(plan["evidence"].toString()+"/batch-"+mode.replace(":","-")+".png"));dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Apply)->click();};
    int shot=0;const auto evidence=plan["evidence"].toString();if(!evidence.isEmpty())QDir().mkpath(evidence);
    for(auto value:plan["edits"].toArray()){const auto edit=value.toObject();qInfo()<<"Screen edit"<<edit["screen"];open(edit["screen"].toString());QStringList ids;for(auto id:edit["ids"].toArray())ids.append(id.toString());choose(ids);auto search=w.findChild<QLineEdit*>("hierarchySearch");search->setText(" ");search->clear();QCOMPARE(tree->selectedItems().size(),ids.size());const auto before=w.projectData();
      QTimer::singleShot(30,&w,[&]{auto d=w.findChild<QDialog*>("selectionEditDialog");QVERIFY(d);fill(d,edit);});w.findChild<QPushButton*>("selectionBatch")->click();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));const auto after=w.projectData();QVERIFY(before!=after);
      w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),after);QCOMPARE(w.selectedIds().size(),ids.size());
      // Cancel is a no-op, and copy/paste of several roots is one undoable edit.
      QTimer::singleShot(30,&w,[&]{auto d=w.findChild<QDialog*>("selectionEditDialog");QVERIFY(d);d->reject();});w.findChild<QPushButton*>("selectionBatch")->click();QCOMPARE(w.projectData(),after);
      w.findChild<QAction*>("copyElements")->trigger();w.findChild<QAction*>("pasteElements")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));QCOMPARE(w.selectedIds().size(),ids.size());QVERIFY(w.projectData()!=after);
      w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),after);choose(ids);
      // Switching back to a single property form must restore text editing.
      choose({ids.first()});auto name=w.findChild<QLineEdit*>("elementName");QVERIFY(name->isEnabled());name->setFocus();name->selectAll();QTest::keyClick(name,Qt::Key_C,Qt::ControlModifier);QCOMPARE(QApplication::clipboard()->text(),name->text());choose(ids);
      if(!evidence.isEmpty())QVERIFY(w.grab().save(evidence+QString("/screen-%1.png").arg(++shot)));
    }
    const auto component=plan["component"].toObject();if(!component.isEmpty()){
      qInfo()<<"Component edit";open(component["screen"].toString());choose({component["instance"].toString()});const auto before=w.projectData();bool edited=false;
      QTimer::singleShot(50,&w,[&]{auto d=w.findChild<QDialog*>("componentsDialog");QVERIFY(d);auto library=d->findChild<QListWidget*>("componentLibrary");QVERIFY(library);QCOMPARE(d->findChild<QLineEdit*>("componentName")->text(),component["label"].toString());auto children=d->findChild<QListWidget*>("componentChildren");children->clearSelection();for(int i=0;i<children->count();++i)for(auto id:component["ids"].toArray())if(children->item(i)->text().startsWith(id.toString()+" ·"))children->item(i)->setSelected(true);
        QTimer::singleShot(30,d,[&]{auto batch=d->findChild<QDialog*>("selectionEditDialog");QVERIFY(batch);fill(batch,component);});d->findChild<QPushButton*>("componentBatch")->click();if(!evidence.isEmpty())QVERIFY(d->grab().save(evidence+"/component.png"));edited=true;d->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
      });w.findChild<QPushButton*>("componentSource")->click();QVERIFY(edited);QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY(w.projectData()!=before);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));
      const auto originalVisual=before["visual"].toObject(),editedVisual=w.projectData()["visual"].toObject();QCOMPARE(editedVisual["screens"],originalVisual["screens"]); // Every instance override is preserved.
      for(auto value:editedVisual["components"].toArray()){const auto c=value.toObject();if(c["label"]!=component["label"])continue;int checked=0;for(auto e:c["elements"].toArray())if(component["ids"].toArray().contains(e.toObject()["id"])) {for(auto key:component["patch"].toObject().keys())QCOMPARE(e.toObject()[key],component["patch"].toObject()[key]);++checked;}QCOMPARE(checked,component["ids"].toArray().size());}
      for(auto expectation:component["expectations"].toArray()){const auto expected=expectation.toObject();bool found=false;for(auto screen:editedVisual["screens"].toArray())if(screen.toObject()["id"]==expected["screen"])for(auto element:canvasElements(editedVisual,screen.toObject()["elements"].toArray(),{editedVisual["width"].toInt(),editedVisual["height"].toInt()}))if(element.toObject()["id"]==expected["id"]){found=true;QCOMPARE(element.toObject()["fontSize"],expected["fontSize"]);}QVERIFY(found);}
      const auto after=w.projectData();w.findChild<QAction*>("undoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),before);w.findChild<QAction*>("redoEdit")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QCOMPARE(w.projectData(),after);
    }
    qInfo()<<"Save reopen validate";const auto authored=w.projectData();w.findChild<QAction*>("saveProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.currentView()["dirty"].toBool(),20000);w.openProject(plan["root"].toString());QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),20000);QCOMPARE(w.projectData(),authored);w.findChild<QAction*>("validateProject")->trigger();QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY(w.findChild<QLabel*>("validationSummary")->text().contains("오류 0건"));
  }
};
QTEST_MAIN(SelectionTests)
#include "selection-test.moc"
