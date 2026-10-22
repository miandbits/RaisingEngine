void StudioTests::toolbarAccessAtSmallSize() {
  StudioWindow w(qEnvironmentVariable("RAISING_ENGINE_ROOT", QDir::currentPath()));
  w.show();
  w.openProject(qEnvironmentVariable("RAISING_QT_PROJECT"));
  QTRY_VERIFY_WITH_TIMEOUT(!w.busy() && !w.projectData().isEmpty(), 20000);
  auto toolbar = w.findChild<QToolBar*>("mainToolbar");
  auto access = w.findChild<QToolButton*>("toolbarAccessButton");
  auto menu = w.findChild<QMenu*>("toolbarAccessMenu");
  QVERIFY(toolbar && access && menu);
  auto last = w.findChild<QAction*>("inspectRenderer");
  auto stop = w.findChild<QAction*>("playProject");
  QVERIFY(last && stop);
  for (const QSize size : {QSize(1000, 680), QSize(1280, 720), QSize(1440, 920)}) {
    w.resize(size);
    QTest::qWait(40);
    qInfo() << "toolbar access size" << size << "actual" << w.size();
    QVERIFY(access->isVisible());
    QVERIFY(toolbar->rect().contains(access->geometry()));
    menu->popup(access->mapToGlobal(QPoint(0, access->height())));
    QTRY_VERIFY(menu->isVisible());
    for (const auto id : {"showAuthoringWorkspace", "relationshipDisplayAction", "saveProject", "validateProject", "showAnalysis", "inspectRenderer"})
      QVERIFY2(menu->actions().contains(w.findChild<QAction*>(id)), id);
    auto locale = menu->findChild<QMenu*>("previewLocaleAccessMenu");
    auto palette = menu->findChild<QMenu*>("elementPaletteAccessMenu");
    QVERIFY(locale && palette);
    QCOMPARE(menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly).size(), 2);
    QVERIFY(palette->actions().contains(w.findChild<QAction*>("add-element-text")));
    QVERIFY(!menu->actions().contains(toolbar->actions().first()));
    menu->close();
  }
  // Exercise the far-right tool through the visible menu, rather than calling
  // its handler. Closing/reopening must preserve the original action binding.
  auto preview = w.findChild<RendererPreviewDock*>();
  QVERIFY(preview);
  QSignalSpy invoked(last, &QAction::triggered);
  menu->popup(access->mapToGlobal(QPoint(0, access->height())));
  QTRY_VERIFY(menu->isVisible());
  const auto rect = menu->actionGeometry(last);
  QVERIFY(!rect.isEmpty());
  QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, rect.center());
  QCOMPARE(invoked.count(), 1);
  QVERIFY(preview->isVisible());
  // Combo-box controls remain usable from the menu and retain activated().
  auto combo = w.findChild<QComboBox*>("previewLocale");
  combo->addItem("Test language", "test");
  const int index = combo->currentIndex();
  QSignalSpy changed(combo, qOverload<int>(&QComboBox::activated));
  menu->popup(access->mapToGlobal(QPoint(0, access->height())));
  auto locale = menu->findChild<QMenu*>("previewLocaleAccessMenu");
  QVERIFY(locale);
  QCOMPARE(locale->actions().size(), combo->count());
  locale->actions()[index]->trigger();
  QCOMPARE(combo->currentIndex(), index);
  QCOMPARE(changed.count(), 1);
  menu->close();
  // Disabled tools and context-only widget actions cannot be enabled by the menu.
  const bool enabled = stop->isEnabled();
  stop->setEnabled(false);
  menu->popup(access->mapToGlobal(QPoint(0, access->height())));
  QVERIFY(!menu->actions()[menu->actions().indexOf(stop)]->isEnabled());
  menu->close();
  stop->setEnabled(enabled);
}
