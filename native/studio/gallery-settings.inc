  auto galleryPage=new QWidget;auto galleryLayout=new QVBoxLayout(galleryPage);tabs->addTab(galleryPage,tr("회상 · 엔딩 도감"));
  auto galleryConfig=project.value("gallery").toObject();
  auto galleryEnabled=new QCheckBox(tr("회차 공통 도감 사용"));galleryEnabled->setObjectName("projectGalleryEnabled");galleryEnabled->setChecked(project.contains("gallery"));galleryLayout->addWidget(galleryEnabled);
  auto galleryTitle=new QLineEdit(galleryConfig.value("title").toString(tr("추억과 결말")));galleryTitle->setObjectName("projectGalleryTitle");galleryTitle->setPlaceholderText(tr("도감 제목"));galleryTitle->setMaxLength(120);galleryLayout->addWidget(galleryTitle);
  auto galleryTable=new QTableWidget(0,6);galleryTable->setObjectName("projectGalleryEntries");galleryTable->setHorizontalHeaderLabels({tr("고정 ID"),tr("사건 / 엔딩"),tr("제목"),tr("분류"),tr("잠금 힌트"),tr("썸네일")});galleryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);galleryTable->setColumnWidth(1,190);galleryTable->setColumnWidth(4,200);galleryTable->setSelectionBehavior(QAbstractItemView::SelectRows);galleryLayout->addWidget(galleryTable);
  auto galleryAddRow=[&](const QJsonObject& entry){
    int row=galleryTable->rowCount();galleryTable->insertRow(row);
    for(const auto& pair:QList<QPair<int,QString>>{{0,"id"},{2,"title"},{3,"category"},{4,"hint"}})galleryTable->setItem(row,pair.first,new QTableWidgetItem(entry.value(pair.second).toString()));
    auto source=new QComboBox;auto thumbnail=new QComboBox;thumbnail->addItem(tr("없음"),"");
    for(auto value:view.value("documents").toArray()){
      auto doc=value.toObject(),data=doc.value("data").toObject();auto kind=doc.value("kind").toString();
      if(kind=="event"||kind=="ending"){auto key=kind+":"+data.value("id").toString();source->addItem((kind=="event"?tr("사건 · "):tr("엔딩 · "))+data.value("label").toString(data.value("id").toString()),key);}
      if(kind=="assets")for(auto asset:data.value("assets").toArray()){auto a=asset.toObject();if(a.value("kind")=="image")thumbnail->addItem(a.value("id").toString(),a.value("id").toString());}
    }
    const auto key=entry.value("kind").toString()+":"+entry.value("sourceId").toString();if(entry.contains("sourceId")&&source->findData(key)<0)source->addItem(tr("없는 참조 · ")+key,key);
    source->setCurrentIndex(entry.contains("sourceId")?source->findData(key):0);
    const auto thumb=entry.value("thumbnailAssetId").toString();if(thumbnail->findData(thumb)<0)thumbnail->addItem(tr("없는 이미지 · ")+thumb,thumb);thumbnail->setCurrentIndex(thumbnail->findData(thumb));
    galleryTable->setCellWidget(row,1,source);galleryTable->setCellWidget(row,5,thumbnail);
  };
  for(auto entry:galleryConfig.value("entries").toArray())galleryAddRow(entry.toObject());
  auto galleryButtons=new QHBoxLayout;auto galleryAdd=new QPushButton(tr("항목 추가"));galleryAdd->setObjectName("projectGalleryAdd");auto galleryRemove=new QPushButton(tr("선택 삭제"));galleryRemove->setObjectName("projectGalleryRemove");galleryButtons->addWidget(galleryAdd);galleryButtons->addWidget(galleryRemove);galleryLayout->addLayout(galleryButtons);
  connect(galleryAdd,&QPushButton::clicked,&dialog,[&]{if(galleryTable->rowCount()>=2000)return;int serial=galleryTable->rowCount()+1;QString id;bool exists;do{id="memory-"+QString::number(serial++);exists=false;for(int i=0;i<galleryTable->rowCount();i++)if(galleryTable->item(i,0)->text()==id)exists=true;}while(exists);galleryAddRow({{"id",id},{"title",tr("새 추억")},{"category",tr("이야기")},{"hint",tr("이 이야기를 끝까지 감상하세요.")}});galleryTable->selectRow(galleryTable->rowCount()-1);});
  connect(galleryRemove,&QPushButton::clicked,&dialog,[&]{const int row=galleryTable->currentRow();if(row>=0)galleryTable->removeRow(row);});
  auto galleryHelp=new QLabel(tr("사건·엔딩을 끝까지 본 뒤 해금됩니다. 행 순서가 도감 순서입니다. 미해금 제목·이미지는 숨기고 힌트만 보여줍니다. 고정 ID는 출시 후 유지하세요. 삭제한 기록은 사용자 프로필에 보존됩니다. 현재 Windows 플레이어에서 지원합니다."));galleryHelp->setWordWrap(true);galleryLayout->addWidget(galleryHelp);
  galleryTable->setEnabled(galleryEnabled->isChecked());connect(galleryEnabled,&QCheckBox::toggled,galleryTable,&QWidget::setEnabled);
  auto readGallery=[&]{QJsonArray entries;for(int row=0;row<galleryTable->rowCount();row++){auto source=qobject_cast<QComboBox*>(galleryTable->cellWidget(row,1))->currentData().toString().split(':');QJsonObject e{{"kind",source.value(0)},{"sourceId",source.value(1)}};for(const auto& pair:QList<QPair<int,QString>>{{0,"id"},{2,"title"},{3,"category"},{4,"hint"}})e[pair.second]=galleryTable->item(row,pair.first)->text().trimmed();auto thumb=qobject_cast<QComboBox*>(galleryTable->cellWidget(row,5))->currentData().toString();if(!thumb.isEmpty())e["thumbnailAssetId"]=thumb;entries.append(e);}return QJsonObject{{"title",galleryTitle->text().trimmed()},{"entries",entries}};};

  auto moveGallery=[&](int delta){const int row=galleryTable->currentRow(),next=row+delta;if(row<0||next<0||next>=galleryTable->rowCount())return;auto values=readGallery().value("entries").toArray();const auto a=values.at(row),b=values.at(next);values[row]=b;values[next]=a;galleryTable->setRowCount(0);for(auto value:values)galleryAddRow(value.toObject());galleryTable->selectRow(next);};
  auto galleryUp=new QPushButton(tr("위로"));galleryUp->setObjectName("projectGalleryUp");auto galleryDown=new QPushButton(tr("아래로"));galleryDown->setObjectName("projectGalleryDown");galleryButtons->addWidget(galleryUp);galleryButtons->addWidget(galleryDown);
  connect(galleryUp,&QPushButton::clicked,&dialog,[&]{moveGallery(-1);});connect(galleryDown,&QPushButton::clicked,&dialog,[&]{moveGallery(1);});
