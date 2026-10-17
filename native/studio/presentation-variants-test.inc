void StudioTests::presentationVariantsAuthoring() {
  const auto engine=qEnvironmentVariable("RAISING_ENGINE_ROOT",QDir::currentPath()),project=qEnvironmentVariable("RAISING_QT_PROJECT");QVERIFY(!project.isEmpty());
  StudioWindow w(engine);w.show();w.openProject(project);QTRY_VERIFY_WITH_TIMEOUT(!w.busy()&&!w.projectData().isEmpty(),20000);
  auto settle=[&]{QTRY_VERIFY_WITH_TIMEOUT(!w.busy(),15000);QVERIFY2(w.lastError.isEmpty(),qPrintable(w.lastError));};
  auto list=w.findChild<QListWidget*>("projectList");QVERIFY(list);bool found=false;for(int i=0;i<list->count();++i)if(list->item(i)->data(Qt::UserRole)=="screen-1"){list->setCurrentRow(i);list->setFocus();QTest::keyClick(list,Qt::Key_Return);found=true;break;}QVERIFY(found);
  auto palette=w.findChild<QToolButton*>("elementPalette");QVERIFY(palette);for(auto a:palette->menu()->actions())if(a->text()=="텍스트"){a->trigger();break;}settle();
  for(auto pair:QList<QPair<const char*,int>>{{"elementX",400},{"elementY",100},{"elementWidth",380},{"elementHeight",50}}){auto field=w.findChild<QSpinBox*>(pair.first);QVERIFY(field);field->setValue(pair.second);QMetaObject::invokeMethod(field,"editingFinished");settle();}
  auto field=w.findChild<QLineEdit*>("buttonText");QVERIFY(field);field->setText("기본 표현");QMetaObject::invokeMethod(field,"editingFinished");settle();
  const auto id=w.selectedId();
  auto selectedElement=[&]{for(const auto& screen:w.projectData().value("visual").toObject().value("screens").toArray())for(const auto& element:screen.toObject().value("elements").toArray())if(element.toObject().value("id")==id)return element.toObject();return QJsonObject();};QTest::mouseClick(w.findChild<QPushButton*>("presentationVariants"),Qt::LeftButton);
  auto dialog=w.findChild<QDialog*>("presentationVariantsDialog");QVERIFY(dialog);
  QTest::mouseClick(dialog->findChild<QPushButton*>("variantAdd"),Qt::LeftButton);auto edit=dialog->findChild<QDialog*>("presentationVariantEdit");QVERIFY(edit);
  edit->findChild<QLineEdit*>("variantName")->setText("휴식 결과의 표현");auto activity=edit->findChild<QComboBox*>("variantActivity");QVERIFY(activity->findData("rest")>=0);activity->setCurrentIndex(activity->findData("rest"));
  auto property=edit->findChild<QComboBox*>("variantProperty");property->setCurrentIndex(property->findData("text"));edit->findChild<QLineEdit*>("variantValue")->setText("휴식 중 · 기운을 회복한다");
  QTest::mouseClick(edit->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok),Qt::LeftButton);
  QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save),Qt::LeftButton);settle();
  auto rules=selectedElement().value("variants").toArray();QCOMPARE(rules.size(),1);QCOMPARE(rules.first().toObject().value("activityIds").toArray().first().toString(),QString("rest"));
  // Reopening and cancelling must preserve the authored rule.
  QTest::mouseClick(w.findChild<QPushButton*>("presentationVariants"),Qt::LeftButton);dialog=w.findChild<QDialog*>("presentationVariantsDialog");QVERIFY(dialog);QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Cancel),Qt::LeftButton);settle();QCOMPARE(selectedElement().value("variants").toArray(),rules);
  w.findChild<QAction*>("saveProject")->trigger();settle();QVERIFY(!w.currentView().value("dirty").toBool());QVERIFY(w.grab().save(QDir(engine).filePath(".cache/designer-freedom/authoring.png")));
  QFile info(QDir(engine).filePath(".cache/designer-freedom/authoring.json"));QVERIFY(info.open(QIODevice::WriteOnly));info.write(QJsonDocument(QJsonObject{{"project",project},{"elementId",id},{"view",w.currentView()}}).toJson());info.close();
}
