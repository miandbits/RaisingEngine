void StudioTests::inspectorFocusRoundtrip() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  const auto root = qEnvironmentVariable("RAISING_QT_PROJECT");
  QVERIFY(!root.isEmpty());
  w.show();
  w.activateWindow();
  QTRY_VERIFY(w.isActiveWindow());
  w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  w.findChild<QAction *>("addButton")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  auto name = w.findChild<QLineEdit *>("elementName");
  auto text = w.findChild<QLineEdit *>("buttonText");
  auto inspector = w.findChild<QWidget *>("inspector");
  QVERIFY(name && text && inspector);
  const auto id = w.selectedId();
  auto element = [&] {
    for (auto screen : w.projectData().value("visual").toObject().value("screens").toArray())
      for (auto value : screen.toObject().value("elements").toArray())
        if (value.toObject().value("id") == id) return value.toObject();
    return QJsonObject();
  };

  const int beforeEnter = w.currentView().value("revision").toInt();
  name->setFocus();
  name->selectAll();
  QTest::keyClicks(name, "Focused button");
  name->setSelection(7, -4); // Preserve a backwards selection as well as focus.
  const int cursor = name->cursorPosition(), selectionStart = name->selectionStart();
  const auto selectedText = name->selectedText();
  QTest::keyClick(name, Qt::Key_Return);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY2(w.lastError.isEmpty(), qPrintable(w.lastError));
  QVERIFY(name->hasFocus());
  QCOMPARE(name->cursorPosition(), cursor);
  QCOMPARE(name->selectionStart(), selectionStart);
  QCOMPARE(name->selectedText(), selectedText);
  QCOMPARE(element().value("label").toString(), QString("Focused button"));
  QCOMPARE(w.currentView().value("revision").toInt(), beforeEnter + 1);

  name->selectAll();
  QTest::keyClicks(name, "Tab destination");
  QTest::keyClick(name, Qt::Key_Tab);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY2(text->hasFocus(), "Committing the name must preserve Tab's destination, not refocus the name");
  QCOMPARE(element().value("label").toString(), QString("Tab destination"));
  const auto oldText = element().value("text").toString();
  text->selectAll();
  QTest::keyClicks(text, "Continued editing");
  QTest::keyClick(text, Qt::Key_Backtab);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(name->hasFocus());
  QCOMPARE(element().value("text").toString(), QString("Continued editing"));
  w.findChild<QAction *>("undoEdit")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(element().value("text").toString(), oldText);
  QCOMPARE(element().value("label").toString(), QString("Tab destination"));
  w.findChild<QAction *>("redoEdit")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(element().value("text").toString(), QString("Continued editing"));

  name->setFocus();
  name->selectAll();
  QTest::keyClicks(name, "Mouse destination");
  QTest::mouseClick(text, Qt::LeftButton);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(text->hasFocus());
  QCOMPARE(element().value("label").toString(), QString("Mouse destination"));
  QVERIFY(inspector->isEnabled());
}

void StudioTests::inspectorFocusDoesNotSteal() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  const auto root = qEnvironmentVariable("RAISING_QT_PROJECT");
  QVERIFY(!root.isEmpty());
  w.show();
  w.activateWindow();
  QTRY_VERIFY(w.isActiveWindow());
  w.openProject(root);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  w.findChild<QAction *>("addButton")->trigger();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  auto name = w.findChild<QLineEdit *>("elementName");
  auto inspector = w.findChild<QWidget *>("inspector");
  QVERIFY(name && inspector);
  auto beginEdit = [&](const QString &value) {
    name->setFocus();
    name->setText(value);
    // Direct signal delivery keeps the test's next action inside the in-flight
    // request without adding a production delay or changing service semantics.
    QMetaObject::invokeMethod(name, "editingFinished", Qt::DirectConnection);
  };

  beginEdit("Move elsewhere during request");
  QVERIFY(w.busy());
  QVERIFY(!inspector->isEnabled());
  QLineEdit elsewhere(&w);
  elsewhere.show();
  elsewhere.setFocus();
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(elsewhere.hasFocus());
  elsewhere.hide();

  beginEdit("Cancel focus restoration");
  QVERIFY(w.busy());
  QTest::keyClick(&w, Qt::Key_Escape);
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(!name->hasFocus());

  const auto originalId = w.selectedId();
  QString otherId;
  for (auto screen : w.projectData().value("visual").toObject().value("screens").toArray())
    if (screen.toObject().value("id") == "home")
      for (auto value : screen.toObject().value("elements").toArray()) {
        const auto id = value.toObject().value("id").toString();
        if (id != originalId) { otherId = id; break; }
      }
  QVERIFY(!otherId.isEmpty());
  auto canvas = w.findChild<ScreenCanvas *>();
  QVERIFY(canvas);
  beginEdit("Selection changed during request");
  QVERIFY(w.busy());
  QVERIFY(QMetaObject::invokeMethod(canvas, "selectionChanged", Qt::DirectConnection, Q_ARG(QString, otherId)));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QCOMPARE(w.selectedId(), otherId);
  QVERIFY(!name->hasFocus());
  QVERIFY(QMetaObject::invokeMethod(canvas, "selectionChanged", Qt::DirectConnection, Q_ARG(QString, originalId)));

  beginEdit("Switch windows during request");
  QVERIFY(w.busy());
  QLineEdit otherWindow;
  otherWindow.show();
  otherWindow.activateWindow();
  otherWindow.setFocus();
  QTRY_VERIFY(otherWindow.isActiveWindow());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(otherWindow.hasFocus());
  QVERIFY(!name->hasFocus());
  otherWindow.hide();
  w.activateWindow();
  QTRY_VERIFY(w.isActiveWindow());

  // A failed replacement must neither refocus the rejected input nor advance
  // revision/history. This exceeds the existing 2 MiB document limit.
  const auto beforeError = w.projectData();
  const int revision = w.currentView().value("revision").toInt();
  name->setMaxLength(3 * 1024 * 1024);
  beginEdit(QString(2 * 1024 * 1024, QLatin1Char('x')));
  QVERIFY(w.busy());
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy(), 10000);
  QVERIFY(!w.lastError.isEmpty());
  QVERIFY(!name->hasFocus());
  QCOMPARE(w.projectData(), beforeError);
  QCOMPARE(w.currentView().value("revision").toInt(), revision);
}
