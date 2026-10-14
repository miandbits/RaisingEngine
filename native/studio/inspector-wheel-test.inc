#include "inspector-wheel.hpp"
#include <QDoubleSpinBox>
#include <QDirIterator>
#include <QScrollBar>
#include <QStyleOptionSpinBox>
#include <QTemporaryDir>
#include <QVBoxLayout>

namespace {
void sendInspectorWheel(QWidget *target, int delta = -120) {
  const QPointF local(target->rect().center());
  QWheelEvent event(local, target->mapToGlobal(local), QPoint(), QPoint(0, delta),
                    Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
  QApplication::sendEvent(target, &event);
}
}

void StudioTests::inspectorWheelScrollsWithoutEditing() {
  QScrollArea area;
  area.setWidgetResizable(true);
  area.resize(320, 220);
  auto form = new QWidget;
  auto layout = new QVBoxLayout(form);
  auto integer = new QSpinBox;
  integer->setRange(0, 200);
  integer->setValue(60);
  auto decimal = new QDoubleSpinBox;
  decimal->setValue(12.5);
  auto combo = new QComboBox;
  combo->addItems({"one", "two", "three"});
  combo->setCurrentIndex(1);
  layout->addWidget(integer);
  layout->addWidget(decimal);
  layout->addWidget(combo);
  layout->addSpacing(1500);
  area.setWidget(form);
  new InspectorWheelGuard(form, &area);
  area.show();
  QTest::qWait(20);
  QVERIFY(area.verticalScrollBar()->maximum() > 0);
  QSignalSpy integers(integer, &QSpinBox::valueChanged);
  QSignalSpy decimals(decimal, &QDoubleSpinBox::valueChanged);
  QSignalSpy selections(combo, &QComboBox::currentIndexChanged);
  const QList<QWidget *> controls{integer, integer->findChild<QLineEdit *>(),
                                 decimal, combo};
  for (auto target : controls) {
    QVERIFY(target);
    for (bool focused : {false, true}) {
      area.verticalScrollBar()->setValue(0);
      if (focused) target->setFocus();
      else target->clearFocus();
      sendInspectorWheel(target);
      QCOMPARE(integer->value(), 60);
      QCOMPARE(decimal->value(), 12.5);
      QCOMPARE(combo->currentIndex(), 1);
      QVERIFY(area.verticalScrollBar()->value() > 0);
    }
  }
  QCOMPARE(integers.count(), 0);
  QCOMPARE(decimals.count(), 0);
  QCOMPARE(selections.count(), 0);
  // The guard must not disable deliberate keyboard or arrow-button edits.
  area.verticalScrollBar()->setValue(0);
  integer->setFocus();
  QTest::keyClick(integer, Qt::Key_Up);
  QCOMPARE(integer->value(), 61);
  QStyleOptionSpinBox option;
  option.initFrom(integer);
  option.rect = integer->rect();
  option.frame = integer->hasFrame();
  option.buttonSymbols = integer->buttonSymbols();
  option.stepEnabled = QAbstractSpinBox::StepUpEnabled | QAbstractSpinBox::StepDownEnabled;
  auto up = integer->style()->subControlRect(QStyle::CC_SpinBox, &option,
                                             QStyle::SC_SpinBoxUp, integer);
  QTest::mouseClick(integer, Qt::LeftButton, Qt::NoModifier, up.center());
  QCOMPARE(integer->value(), 62);
  QTest::keyClick(combo, Qt::Key_Down);
  QCOMPARE(combo->currentIndex(), 2);
}

void StudioTests::inspectorWheelDoesNotChangeProject() {
  const auto engine = qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath());
  QTemporaryDir temp(engine + "/.cache/qt-inspector-wheel-XXXXXX");
  QVERIFY(temp.isValid());
  const auto source = engine + "/examples/academy-adventure";
  QVERIFY(QDir(source).exists());
  QDirIterator files(source, QDir::Files, QDirIterator::Subdirectories);
  while (files.hasNext()) {
    const auto path = files.next();
    const auto destination = temp.path() + "/" + QDir(source).relativeFilePath(path);
    QVERIFY(QDir().mkpath(QFileInfo(destination).path()));
    QVERIFY(QFile::copy(path, destination));
  }
  StudioWindow window(engine);
  window.show();
  window.openProject(temp.path());
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.projectData().isEmpty(), 20000);
  window.findChild<QAction *>("addButton")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy(), 10000);
  window.findChild<QAction *>("saveProject")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!window.busy() && !window.currentView().value("dirty").toBool(), 10000);
  auto height = window.findChild<QSpinBox *>("elementHeight");
  auto anchor = window.findChild<QComboBox *>("anchorY");
  QVERIFY(height && anchor && height->isEnabled());
  const auto project = window.projectData();
  const auto revision = window.currentView().value("revision");
  const auto initialHeight = height->value();
  const auto initialAnchor = anchor->currentIndex();
  height->setFocus();
  sendInspectorWheel(height, 1560);
  sendInspectorWheel(height->findChild<QLineEdit *>(), -1560);
  anchor->setFocus();
  sendInspectorWheel(anchor);
  QTest::qWait(100);
  QCOMPARE(height->value(), initialHeight);
  QCOMPARE(anchor->currentIndex(), initialAnchor);
  QCOMPARE(window.currentView().value("revision"), revision);
  QVERIFY(!window.currentView().value("dirty").toBool());
  QCOMPARE(window.projectData(), project);
}
