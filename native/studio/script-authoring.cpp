#include "script-authoring.hpp"
#include "story-connections.hpp"
#include "script-rounds.hpp"
#include "script-continuation.hpp"
#include "script-identity.hpp"
#include "script-graph.hpp"
#include <QApplication>
#include <QFontMetrics>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QCheckBox>
#include <QListView>
#include "studio.hpp"
#include "logic-editor.hpp"
#include "renderer-preview.hpp"
#include <QPointer>
#include <QTimer>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QHeaderView>
#include <QJsonDocument>
#include <QPainterPath>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int identityRole = Qt::UserRole + 4;
class StoryIdentityDelegate final : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override { return {720,72}; }
  void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
    QStyleOptionViewItem o(option);initStyleOption(&o,index);o.text.clear();
    const auto style=o.widget?o.widget->style():QApplication::style();style->drawControl(QStyle::CE_ItemViewItem,&o,p,o.widget);
    p->save();p->setClipRect(option.rect);const auto area=option.rect.adjusted(10,4,-10,-4);
    p->setPen(option.palette.color(option.state&QStyle::State_Selected?QPalette::HighlightedText:QPalette::Text));
    QFont title=option.font;title.setBold(true);p->setFont(title);
    p->drawText(area,Qt::AlignLeft|Qt::AlignTop,QFontMetrics(title).elidedText(index.data().toString(),Qt::ElideRight,area.width()));
    p->setFont(option.font);const auto lines=index.data(identityRole).toString().split('\n');
    for(int i=0;i<lines.size();++i)p->drawText(area.adjusted(0,23+i*19,0,0),Qt::AlignLeft|Qt::AlignTop,QFontMetrics(option.font).elidedText(lines[i],Qt::ElideRight,area.width()));
    p->restore();
  }
};
bool validId(QString id){return id.size()<=64&&QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(id).hasMatch();}
QString uniqueId(QJsonArray values,QString prefix){QSet<QString> ids;for(auto v:values)ids.insert(v.toObject().value("id").toString());for(int n=1;;++n){auto id=prefix+QString::number(n);if(!ids.contains(id))return id;}}
QStringList links(QJsonObject n){const auto type=n.value("type").toString();QStringList out;if(QStringList{"scene","dialogue","effect"}.contains(type))out<<n.value("next").toString();if(type=="branch")out<<n.value("then").toString()<<n.value("else").toString();if(type=="choice")for(auto c:n.value("choices").toArray())out<<c.toObject().value("next").toString();return out;}
QList<QPair<QString,QString>> labelledLinks(QJsonObject n){QList<QPair<QString,QString>> result;if(n.value("type")=="branch")return {{n.value("then").toString(),QObject::tr("참")},{n.value("else").toString(),QObject::tr("거짓")}};if(n.value("type")=="choice"){for(auto v:n.value("choices").toArray()){auto c=v.toObject();result.append({c.value("next").toString(),c.value("text").toString()});}return result;}for(auto to:links(n))result.append({to,{}});return result;}
QJsonObject renameLinks(QJsonObject n,QString from,QString to){for(auto key:{"next","then","else"})if(n.value(key)==from)n[key]=to;if(n.value("type")=="choice"){auto a=n.value("choices").toArray();for(int i=0;i<a.size();++i){auto c=a[i].toObject();if(c.value("next")==from)c["next"]=to;a[i]=c;}n["choices"]=a;}return n;}
QPushButton *button(QBoxLayout *row,QString id,QString text){auto b=new QPushButton(text);b->setObjectName(id);row->addWidget(b);return b;}
QJsonObject choice(QString id,QString next){return {{"id",id},{"text",QObject::tr("새 선택지")},{"requirements",QJsonObject{{"op","always"}}},{"effects",QJsonArray{}},{"next",next}};}
QString nodeLabel(QJsonObject n){
  static const QMap<QString,QString> names{{"scene",QObject::tr("장면")},{"dialogue",QObject::tr("대사")},{"choice",QObject::tr("선택")},{"branch",QObject::tr("조건 분기")},{"effect",QObject::tr("효과")},{"end",QObject::tr("종료")}};
  const auto excerpt=n.value("text").toString(n.value("prompt").toString()).simplified().left(50);
  return names.value(n.value("type").toString(),n.value("type").toString())+" · "+(excerpt.isEmpty()?n.value("id").toString():excerpt);
}
}

QStringList scriptGraphIssues(const QJsonObject &script){
  QStringList issues;const auto nodes=script.value("nodes").toArray();QSet<QString> ids;QMap<QString,QStringList> edges;
  if(nodes.isEmpty())issues<<QObject::tr("대본에는 노드가 하나 이상 필요합니다.");
  for(auto v:nodes){const auto n=v.toObject();auto id=n.value("id").toString();if(!validId(id)||ids.contains(id))issues<<QObject::tr("중복되거나 올바르지 않은 노드 ID: ")+id;ids.insert(id);edges[id]=links(n);
    const auto type=n.value("type").toString();if(type=="dialogue"&&n.value("text").toString().trimmed().isEmpty())issues<<id+QObject::tr(": 대사가 비어 있습니다.");
    if(type=="scene"&&n.value("scene").toObject().isEmpty())issues<<id+QObject::tr(": 장면 설정이 필요합니다.");
    if(type=="choice"){QSet<QString> choices;const auto values=n.value("choices").toArray();if(values.isEmpty()||n.value("prompt").toString().trimmed().isEmpty())issues<<id+QObject::tr(": 질문과 선택지가 필요합니다.");for(auto c:values){const auto o=c.toObject();auto cid=o.value("id").toString();if(!validId(cid)||choices.contains(cid)||o.value("text").toString().trimmed().isEmpty())issues<<id+QObject::tr(": 선택지 ID·문구를 확인하세요: ")+cid;choices.insert(cid);}}
  }
  if(!ids.contains(script.value("entryNodeId").toString()))issues<<QObject::tr("시작 노드를 선택하세요.");
  for(auto it=edges.begin();it!=edges.end();++it)for(auto to:it.value())if(!ids.contains(to))issues<<it.key()+QObject::tr(" → 없는 연결: ")+to;
  QSet<QString> active,done;std::function<bool(QString)> cycle=[&](QString id){if(active.contains(id))return true;if(done.contains(id)||!ids.contains(id))return false;active.insert(id);for(auto to:edges.value(id))if(cycle(to))return true;active.remove(id);done.insert(id);return false;};
  for(auto id:ids)if(cycle(id)){issues<<QObject::tr("순환 연결이 있습니다: ")+id;break;}return issues;
}

ScriptAuthoringDialog::ScriptAuthoringDialog(QJsonObject ctx,QJsonArray images,QWidget *parent,std::function<QString()> guard,std::function<void(QJsonObject,std::function<void(QJsonObject)>)> preview):QDialog(parent),context(ctx),assets(images),saveGuard(std::move(guard)),scenePreview(std::move(preview)){
  for(auto v:ctx.value("documents").toArray()){const auto d=v.toObject();if(d.value("kind")=="event"||d.value("kind")=="ending")documents.append(d);}original=documents;for(auto v:ctx.value("documents").toArray())if(v.toObject().value("kind")=="scenario")externalDocuments.append(v);externalOriginal=externalDocuments;
  setObjectName("scriptAuthoringDialog");setWindowTitle(tr("사건 · 조건 · 결말 작업실"));resize(1380,880);
  auto outer=new QVBoxLayout(this);auto help=new QLabel(tr("장면·대사·선택지·조건 분기·효과를 연결하세요. 시작 노드와 모든 연결을 검사하고, 적용한 문서들은 한 번의 실행 취소로 되돌릴 수 있습니다."));help->setWordWrap(true);outer->addWidget(help);
  auto docRow=new QHBoxLayout;outer->addLayout(docRow);documentField=new QComboBox;documentField->setObjectName("scriptDocument");for(auto v:documents){const auto d=v.toObject(),s=d.value("data").toObject();documentField->addItem(s.value("label").toString(s.value("id").toString()));}docRow->addWidget(new QLabel(tr("사건·엔딩")));documentField->setItemDelegate(new StoryIdentityDelegate(documentField));documentField->view()->setMinimumWidth(740);documentField->setMaxVisibleItems(9);documentField->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);documentField->setMinimumContentsLength(20);docRow->addWidget(documentField,1);entryField=new QComboBox;entryField->setObjectName("scriptEntry");docRow->addWidget(new QLabel(tr("시작 노드")));docRow->addWidget(entryField);
  auto findRow=new QHBoxLayout;
  auto search=new QLineEdit;search->setObjectName("scriptSearch");search->setStyleSheet("QLineEdit { placeholder-text-color: #a5b3bb; }");search->setPlaceholderText(tr("사건·인물·시기·선행 사건·대사 검색"));
  auto continuation=new QPushButton(tr("앞선 사건 연결…"));continuation->setObjectName("scriptConnectPredecessor");
  auto connections=new QPushButton(tr("사건 연결 지도…"));connections->setObjectName("scriptConnections");
  connect(connections,&QPushButton::clicked,this,&ScriptAuthoringDialog::openConnections);
  findRow->addWidget(search,1);findRow->addWidget(connections);findRow->addWidget(continuation);outer->addLayout(findRow);
  connect(search,&QLineEdit::textChanged,this,[this](const QString &text){
    int first=-1;auto list=qobject_cast<QListView*>(documentField->view());
    for(int i=0;i<documents.size();++i){const auto d=documents[i].toObject().value("data").toObject();bool match=text.trimmed().isEmpty()||d.value("label").toString().contains(text,Qt::CaseInsensitive)||scriptIdentity(documents[i].toObject(),documents,context.value("documents").toArray()).contains(text,Qt::CaseInsensitive);
      for(auto v:d.value("nodes").toArray())if(v.toObject().value("text").toString().contains(text,Qt::CaseInsensitive))match=true;
      if(list)list->setRowHidden(i,!match);if(match&&first<0)first=i;}
    if(first>=0)documentField->setCurrentIndex(first);
    if(error)error->setText(first<0?tr("검색 결과가 없습니다. 검색어를 바꾸세요."):QString());
  });
  connect(continuation,&QPushButton::clicked,this,&ScriptAuthoringDialog::connectPredecessor);
  auto showMetadata=new QCheckBox(tr("사건 설정 · 이름·시기·조건"));showMetadata->setObjectName("scriptMetadataToggle");outer->addWidget(showMetadata);metadata=makeMetadata();outer->addWidget(metadata);metadata->hide();connect(showMetadata,&QCheckBox::toggled,metadata,&QWidget::setVisible);auto identity=new QLabel;identity->setObjectName("scriptIdentitySummary");identity->setTextFormat(Qt::PlainText);identity->setWordWrap(true);identity->setStyleSheet("color:#c4d9d0;padding:4px;background:#1d3032");outer->addWidget(identity);
  auto split=new QSplitter;split->setObjectName("scriptWorkspaceSplit");split->setChildrenCollapsible(false);outer->addWidget(split,1);auto left=new QWidget;auto leftBox=new QVBoxLayout(left);leftBox->setContentsMargins(0,0,0,0);nodeSearch=new QLineEdit;nodeSearch->setObjectName("scriptNodeSearch");nodeSearch->setStyleSheet("QLineEdit { placeholder-text-color: #a5b3bb; }");nodeSearch->setPlaceholderText(tr("이 대본에서 대사·선택 찾기"));leftBox->addWidget(nodeSearch);nodeList=new QListWidget;nodeList->setObjectName("scriptNodes");leftBox->addWidget(nodeList,1);auto row=new QHBoxLayout;leftBox->addLayout(row);kindField=new QComboBox;kindField->setObjectName("scriptNodeKind");for(const auto &p:QList<QPair<QString,QString>>{{"scene",tr("장면")},{"dialogue",tr("대사")},{"choice",tr("선택지")},{"branch",tr("조건 분기")},{"effect",tr("효과")},{"end",tr("종료")}})kindField->addItem(p.second,p.first);row->addWidget(kindField);auto add=button(row,"scriptNodeAdd",tr("추가"));auto controls=new QHBoxLayout;leftBox->addLayout(controls);auto duplicate=button(controls,"scriptNodeDuplicate",tr("복제")),remove=button(controls,"scriptNodeRemove",tr("삭제"));split->addWidget(left);
  graph=new ScriptGraphView;graph->setObjectName("scriptGraph");graphScene=new QGraphicsScene(graph);graph->setScene(graphScene);graph->setRenderHint(QPainter::Antialiasing);graph->setDragMode(QGraphicsView::ScrollHandDrag);graph->setBackgroundBrush(QColor("#17232a"));graph->setMinimumWidth(280);split->addWidget(graph);
  auto scroll=new QScrollArea;scroll->setMinimumWidth(300);scroll->setWidgetResizable(true);properties=new QWidget;auto propertiesBox=new QVBoxLayout(properties);propertiesBox->setContentsMargins(0,0,0,0);form=new QFormLayout;form->setFormAlignment(Qt::AlignLeft|Qt::AlignTop);form->setLabelAlignment(Qt::AlignLeft|Qt::AlignTop);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);propertiesBox->addLayout(form);propertiesBox->addStretch(1);scroll->setWidget(properties);split->addWidget(scroll);split->setSizes({230,720,360});split->setStretchFactor(1,1);
  auto navigation=new QHBoxLayout;graphOverview=new ScriptGraphOverview(graph);navigation->addWidget(graphOverview);auto navText=new QVBoxLayout;graphPosition=new QLabel;graphPosition->setObjectName("scriptGraphPosition");graphPosition->setTextFormat(Qt::PlainText);graphPosition->setWordWrap(true);navText->addWidget(graphPosition);auto navHelp=new QLabel(tr("위에서 아래로 읽습니다. 전체 지도를 클릭해 이동 · Ctrl+휠로 확대/축소"));navHelp->setWordWrap(true);navText->addWidget(navHelp);auto focus=new QPushButton(tr("현재 노드 보기 · 100%"));focus->setObjectName("scriptGraphFocus");navText->addWidget(focus);navigation->addLayout(navText,1);outer->addLayout(navigation);connect(focus,&QPushButton::clicked,this,&ScriptAuthoringDialog::focusGraphNode);static_cast<ScriptGraphView*>(graph)->viewChanged=[this]{syncGraphSelection();};
  connect(nodeSearch,&QLineEdit::textChanged,this,[this](QString query){
    int first=-1;
    const auto nodes=data().value("nodes").toArray();
    for(int i=0;i<nodeList->count();++i){
      const auto n=nodes[i].toObject();
      QString text=nodeLabel(n)+" "+n.value("text").toString()+" "+n.value("prompt").toString();
      for(const auto option:n.value("choices").toArray())text+=" "+option.toObject().value("text").toString();
      const bool match=text.contains(query,Qt::CaseInsensitive);
      nodeList->item(i)->setHidden(!match);if(match&&first<0)first=i;
    }
    if(first>=0){nodeList->setCurrentRow(first);focusGraphNode();}
    else graphPosition->setText(tr("이 대본에 일치하는 노드가 없습니다."));
  });
  auto graphControls=new QHBoxLayout;outer->addLayout(graphControls);auto fit=button(graphControls,"scriptGraphFit",tr("흐름 전체 보기"));auto zoomIn=button(graphControls,"scriptGraphZoomIn",tr("확대")),zoomOut=button(graphControls,"scriptGraphZoomOut",tr("축소"));auto insert=button(graphControls,"scriptInsertAfter",tr("선택한 대사·연출 뒤에 대사 추가"));connect(insert,&QPushButton::clicked,this,&ScriptAuthoringDialog::insertDialogueAfter);auto rounds=button(graphControls,"scriptRounds",tr("라운드 흐름 추가…"));connect(rounds,&QPushButton::clicked,this,&ScriptAuthoringDialog::addRounds);graphControls->addStretch();
  error=new QLabel;error->setObjectName("scriptError");error->setWordWrap(true);error->setStyleSheet("color:#ed987f");outer->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);buttons->setObjectName("scriptButtons");outer->addWidget(buttons);
  connect(documentField,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int i){loadDocument(i);});connect(entryField,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{if(filling||documentIndex<0)return;auto d=data();d["entryNodeId"]=entryField->currentData().toString();setData(d);refreshGraph();});
  connect(nodeList,&QListWidget::itemSelectionChanged,this,[this]{const auto rows=nodeList->selectedItems();if(rows.size()==1&&nodeList->currentItem()!=rows.first())nodeList->setCurrentItem(rows.first());});
  connect(nodeList,&QListWidget::currentRowChanged,this,[this](int i){if(filling)return;nodeIndex=i;fillProperties();syncGraphSelection(true);});connect(add,&QPushButton::clicked,this,&ScriptAuthoringDialog::addNode);connect(duplicate,&QPushButton::clicked,this,&ScriptAuthoringDialog::duplicateNode);connect(remove,&QPushButton::clicked,this,&ScriptAuthoringDialog::removeNode);
  connect(fit,&QPushButton::clicked,this,[this]{graph->fitInView(graphScene->sceneRect(),Qt::KeepAspectRatio);syncGraphSelection();});connect(zoomIn,&QPushButton::clicked,this,[this]{static_cast<ScriptGraphView*>(graph)->zoomBy(1.2);});connect(zoomOut,&QPushButton::clicked,this,[this]{static_cast<ScriptGraphView*>(graph)->zoomBy(1/1.2);});
  connect(graphScene,&QGraphicsScene::selectionChanged,this,[this]{if(filling)return;const auto values=graphScene->selectedItems();if(values.isEmpty())return;auto id=values.first()->data(0).toString();QTimer::singleShot(0,this,[this,id]{for(int i=0;i<nodeList->count();++i)if(nodeList->item(i)->data(Qt::UserRole)==id){nodeList->setCurrentRow(i);break;}});});
  connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,this,[this]{if(saveGuard){const auto message=saveGuard();if(!message.isEmpty()){error->setText(message);return;}}if(!commitId())return;for(auto v:documents){auto d=v.toObject().value("data").toObject();auto w=d.value("window").toObject();if(d.value("label").toString().trimmed().isEmpty()||(d.contains("window")&&w.value("fromPeriod").toInt()>w.value("throughPeriod").toInt())){error->setText(tr("표시 이름을 입력하고 발생 시작 일정을 마지막 일정 이하로 설정하세요."));return;}}if(edits().size()>100){error->setText(tr("한 번에 최대 100개 문서를 저장할 수 있습니다. 변경을 줄인 뒤 다시 저장하세요. 현재 편집 내용은 유지됩니다."));return;}for(int i=0;i<documents.size();++i){if(documents[i]==original[i])continue;const auto issues=scriptGraphIssues(documents[i].toObject().value("data").toObject());if(!issues.isEmpty()){documentField->setCurrentIndex(i);error->setText(issues.join('\n'));return;}}
    if(applyEdits){QPointer<ScriptAuthoringDialog> self(this);setEnabled(false);applyEdits(edits(),[self](QString problem){if(!self)return;self->setEnabled(true);if(problem.isEmpty())self->accept();else self->error->setText(problem);});}
    else accept();});
  loadDocument(documents.isEmpty()?-1:0);
}
QJsonObject ScriptAuthoringDialog::data()const{return documentIndex<0?QJsonObject{}:documents[documentIndex].toObject().value("data").toObject();}
void ScriptAuthoringDialog::addRounds(){if(documentIndex<0||!commitId())return;ScriptRoundsDialog dialog(context,data(),node().value("id").toString(),this);if(dialog.exec()!=QDialog::Accepted)return;setData(dialog.resultScript());refreshNodes();}
QJsonObject ScriptAuthoringDialog::node()const{const auto a=data().value("nodes").toArray();return nodeIndex<0||nodeIndex>=a.size()?QJsonObject{}:a[nodeIndex].toObject();}
void ScriptAuthoringDialog::setData(QJsonObject value){if(documentIndex<0)return;auto d=documents[documentIndex].toObject();d["data"]=value;documents[documentIndex]=d;refreshDocumentIdentities();}
void ScriptAuthoringDialog::setNode(QJsonObject value){auto d=data();auto a=d.value("nodes").toArray();if(nodeIndex<0||nodeIndex>=a.size())return;a[nodeIndex]=value;d["nodes"]=a;setData(d);nodeList->item(nodeIndex)->setText(nodeLabel(value));refreshGraph();}
void ScriptAuthoringDialog::loadDocument(int index){documentIndex=index;nodeIndex=-1;{QSignalBlocker b(nodeSearch);nodeSearch->clear();}refreshMetadata();refreshNodes(data().value("entryNodeId").toString());refreshDocumentIdentities();focusGraphNode();}
void ScriptAuthoringDialog::refreshNodes(QString selected){filling=true;nodeList->clear();entryField->clear();const auto d=data();int row=0;for(auto v:d.value("nodes").toArray()){auto n=v.toObject();auto id=n.value("id").toString();auto item=new QListWidgetItem(nodeLabel(n),nodeList);item->setData(Qt::UserRole,id);entryField->addItem(nodeLabel(n),id);if(id==selected)row=nodeList->count()-1;}auto entry=d.value("entryNodeId").toString();if(entryField->findData(entry)<0)entryField->addItem(tr("없는 노드: ")+entry,entry);entryField->setCurrentIndex(entryField->findData(entry));nodeList->setCurrentRow(nodeList->count()?row:-1);nodeIndex=nodeList->currentRow();filling=false;fillProperties();refreshGraph();}
void ScriptAuthoringDialog::addNode(){if(documentIndex<0)return;auto d=data();auto a=d.value("nodes").toArray();const auto type=kindField->currentData().toString();QString end;for(auto v:a)if(v.toObject().value("type")=="end"){end=v.toObject().value("id").toString();break;}if(type!="end"&&end.isEmpty()){end=uniqueId(a,"end-");a.append(QJsonObject{{"id",end},{"type","end"}});}const auto id=uniqueId(a,type+"-");QJsonObject n{{"id",id},{"type",type}};if(type=="scene")n["scene"]=QJsonObject{{"backgroundAssetId",QJsonValue::Null}};if(type=="dialogue"){n["speakerId"]=QJsonValue::Null;n["text"]=tr("새 대사");}if(type=="choice"){n["prompt"]=tr("어떻게 할까요?");n["choices"]=QJsonArray{choice("choice-1",end)};}if(type=="branch"){n["condition"]=QJsonObject{{"op","always"}};n["then"]=end;n["else"]=end;}if(type=="effect")n["effects"]=QJsonArray{};if(QStringList{"scene","dialogue","effect"}.contains(type))n["next"]=end;a.append(n);d["nodes"]=a;if(d.value("entryNodeId").toString().isEmpty())d["entryNodeId"]=id;setData(d);refreshNodes(id);}
void ScriptAuthoringDialog::duplicateNode(){if(node().isEmpty())return;auto d=data();auto a=d.value("nodes").toArray();auto n=node();auto id=uniqueId(a,"copy-");n["id"]=id;a.append(n);d["nodes"]=a;setData(d);refreshNodes(id);}
void ScriptAuthoringDialog::removeNode(){if(node().isEmpty())return;auto d=data();auto a=d.value("nodes").toArray();auto id=node().value("id").toString();QStringList references=externalReferences(id);if(d.value("entryNodeId")==id)references<<tr("시작 노드");for(auto v:a)if(links(v.toObject()).contains(id))references<<v.toObject().value("id").toString();if(a.size()<=1||!references.isEmpty()){error->setText(tr("먼저 이 노드로 향하는 연결을 바꾸세요: ")+references.join(", "));return;}a.removeAt(nodeIndex);d["nodes"]=a;setData(d);refreshNodes();}
bool ScriptAuthoringDialog::commitId(){if(!idField||node().isEmpty())return true;auto from=node().value("id").toString(),to=idField->text().trimmed();if(from==to)return true;auto d=data();auto a=d.value("nodes").toArray();if(!validId(to)){error->setText(tr("노드 ID는 64자 이하 영문 소문자·숫자·하이픈이어야 합니다."));return false;}for(auto v:a)if(v.toObject().value("id")==to){error->setText(tr("이미 사용 중인 노드 ID입니다."));return false;}for(int i=0;i<a.size();++i){auto n=renameLinks(a[i].toObject(),from,to);if(i==nodeIndex)n["id"]=to;a[i]=n;}d["nodes"]=a;if(d.value("entryNodeId")==from)d["entryNodeId"]=to;setData(d);renameExternalReferences(from,to);refreshNodes(to);return true;}

void ScriptAuthoringDialog::fillProperties(){
  filling=true;idField=nullptr;while(form->rowCount())form->removeRow(0);const auto n=node();if(n.isEmpty()){form->addRow(new QLabel(tr("사건·엔딩을 선택하고 노드를 추가하세요.")));filling=false;return;}const auto type=n.value("type").toString();
  idField=new QLineEdit(n.value("id").toString());idField->setObjectName("scriptNodeId");idField->setMaxLength(64);auto advanced=new QCheckBox(tr("고급 연결 식별자 표시"));advanced->setObjectName("scriptAdvancedIds");form->addRow(advanced);auto idLabel=new QLabel(tr("노드 ID"));form->addRow(idLabel,idField);idLabel->hide();idField->hide();auto rename=new QPushButton(tr("ID 변경 · 연결 갱신"));rename->setObjectName("scriptNodeRename");form->addRow(rename);rename->hide();connect(advanced,&QCheckBox::toggled,this,[idField=idField,idLabel,rename](bool on){idField->setVisible(on);idLabel->setVisible(on);rename->setVisible(on);});connect(rename,&QPushButton::clicked,this,[this]{commitId();});auto typeLabel=new QLabel(type);typeLabel->setObjectName("scriptNodeType");typeLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);form->addRow(tr("종류"),typeLabel);
  auto text=[&](QString key,QString label){auto field=new QPlainTextEdit(n.value(key).toString());field->setObjectName("script-"+key);field->setMaximumHeight(140);form->addRow(label,field);connect(field,&QPlainTextEdit::textChanged,this,[this,field,key]{auto n=node();n[key]=field->toPlainText();setNode(n);});};
  auto link=[&](QString key,QString label){auto c=new QComboBox;c->setObjectName("script-"+key);for(auto v:data().value("nodes").toArray()){auto id=v.toObject().value("id").toString();c->addItem(nodeLabel(v.toObject()),id);}const auto selected=n.value(key).toString();if(c->findData(selected)<0)c->addItem(tr("없는 노드: ")+selected,selected);c->setCurrentIndex(c->findData(selected));form->addRow(label,c);connect(c,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,c,key]{auto n=node();n[key]=c->currentData().toString();setNode(n);});};
  auto logic=[&](QString key,QString kind,QString label){auto b=new QPushButton(label);b->setObjectName("script-"+key);form->addRow(b);connect(b,&QPushButton::clicked,this,[this,key,kind]{LogicEditor editor(node().value(key),kind,context,this);if(editor.exec()==QDialog::Accepted){auto n=node();n[key]=editor.value();setNode(n);}});};
  if(type=="dialogue"){
    auto speaker=new QComboBox;speaker->setObjectName("script-speakerId");speaker->addItem(tr("내레이션"),QString());for(auto v:context.value("documents").toArray()){const auto d=v.toObject(),c=d.value("data").toObject();if(d.value("kind")=="character")speaker->addItem(c.value("name").toString()+" · "+c.value("id").toString(),c.value("id").toString());}auto selected=n.value("speakerId").toString();if(speaker->findData(selected)<0)speaker->addItem(tr("없는 인물: ")+selected,selected);speaker->setCurrentIndex(speaker->findData(selected));form->addRow(tr("말하는 인물"),speaker);connect(speaker,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,speaker]{auto n=node();auto id=speaker->currentData().toString();n["speakerId"]=id.isEmpty()?QJsonValue(QJsonValue::Null):QJsonValue(id);setNode(n);});text("text",tr("대사"));link("next",tr("다음 노드"));
  }else if(type=="branch"){logic("condition","condition",tr("분기 조건 편집…"));link("then",tr("조건 참 →"));link("else",tr("조건 거짓 →"));}
  else if(type=="effect"){logic("effects","effects",tr("적용할 효과 편집…"));link("next",tr("다음 노드"));}
  else if(type=="scene"){
    const auto scene=n.value("scene").toObject();for(auto key:{QString("backgroundAssetId"),QString("musicAssetId"),QString("soundAssetId")}){auto c=new QComboBox;c->setObjectName("script-"+key);c->addItem(tr("이 항목은 변경하지 않음"),"__keep__");if(key!="soundAssetId")c->addItem(tr("지우기"),"__clear__");QJsonArray available=assets;for(auto doc:context.value("documents").toArray())if(doc.toObject().value("kind")=="assets")available+=doc.toObject().value("data").toObject().value("assets").toArray();QSet<QString> seen;for(auto v:available){auto a=v.toObject();auto id=a.value("id").toString();if(a.value("kind")!=(key=="backgroundAssetId"?"image":"audio")||seen.contains(id))continue;seen.insert(id);c->addItem(nodeLabel(v.toObject()),id);}QString value=!scene.contains(key)?"__keep__":scene.value(key).isNull()?"__clear__":scene.value(key).toString();if(c->findData(value)<0)c->addItem(tr("없는 에셋: ")+value,value);c->setCurrentIndex(c->findData(value));form->addRow(key=="backgroundAssetId"?tr("배경"):key=="musicAssetId"?tr("배경 음악"):tr("효과음"),c);connect(c,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,c,key]{auto n=node();auto scene=n.value("scene").toObject();auto value=c->currentData().toString();if(value=="__keep__")scene.remove(key);else scene[key]=value=="__clear__"?QJsonValue(QJsonValue::Null):QJsonValue(value);n["scene"]=scene;setNode(n);});}
    auto portraits=new QPushButton(tr("인물 · 표정 · 배치 편집…"));portraits->setObjectName("scriptPortraits");form->addRow(portraits);connect(portraits,&QPushButton::clicked,this,&ScriptAuthoringDialog::editPortraits);auto preview=new QPushButton(tr("선택 장면 미리보기"));preview->setObjectName("scriptScenePreview");preview->setEnabled(bool(scenePreview));form->addRow(preview);connect(preview,&QPushButton::clicked,this,&ScriptAuthoringDialog::previewScene);link("next",tr("다음 노드"));
  }else if(type=="choice"){
    text("prompt",tr("질문"));auto choices=new QTableWidget(0,4);choices->setObjectName("scriptChoices");choices->setHorizontalHeaderLabels({tr("ID"),tr("문구"),tr("다음"),tr("결과 안내")});choices->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);form->addRow(choices);const auto values=n.value("choices").toArray();
    for(int i=0;i<values.size();++i){const auto c=values[i].toObject();choices->insertRow(i);choices->setItem(i,0,new QTableWidgetItem(c.value("id").toString()));choices->setItem(i,1,new QTableWidgetItem(c.value("text").toString()));choices->setItem(i,3,new QTableWidgetItem(c.value("consequenceText").toString()));auto next=new QComboBox;for(auto v:data().value("nodes").toArray()){auto id=v.toObject().value("id").toString();next->addItem(id,id);}auto id=c.value("next").toString();if(next->findData(id)<0)next->addItem(tr("없는 노드: ")+id,id);next->setCurrentIndex(next->findData(id));choices->setCellWidget(i,2,next);connect(next,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,next,i]{auto n=node();auto a=n.value("choices").toArray();if(i>=a.size())return;auto c=a[i].toObject();c["next"]=next->currentData().toString();a[i]=c;n["choices"]=a;setNode(n);});}
    connect(choices,&QTableWidget::cellChanged,this,[this,choices](int row,int col){if(col==2)return;auto n=node();auto a=n.value("choices").toArray();auto c=a[row].toObject();if(col==0){const auto old=c.value("id").toString(),next=choices->item(row,col)->text().trimmed();bool duplicate=false;for(int i=0;i<a.size();++i)if(i!=row&&a[i].toObject().value("id")==next)duplicate=true;if(!validId(next)||duplicate){QSignalBlocker block(choices);choices->item(row,col)->setText(old);error->setText(tr("선택지 ID는 중복 없는 영문 소문자·숫자·하이픈이어야 합니다."));return;}renameExternalReferences(n.value("id").toString(),n.value("id").toString(),old,next);c["id"]=next;}else if(col==1)c["text"]=choices->item(row,col)->text();else {const auto hint=choices->item(row,col)->text();if(hint.isEmpty())c.remove("consequenceText");else c["consequenceText"]=hint;}a[row]=c;n["choices"]=a;setNode(n);});
    auto buttons=new QWidget;auto row=new QHBoxLayout(buttons);row->setContentsMargins(0,0,0,0);form->addRow(buttons);auto add=button(row,"scriptChoiceAdd",tr("선택지 추가")),remove=button(row,"scriptChoiceRemove",tr("삭제"));
    connect(add,&QPushButton::clicked,this,[this]{auto n=node();auto a=n.value("choices").toArray();QString next;for(auto v:data().value("nodes").toArray())if(v.toObject().value("type")=="end"){next=v.toObject().value("id").toString();break;}a.append(choice(uniqueId(a,"choice-"),next));n["choices"]=a;setNode(n);fillProperties();});connect(remove,&QPushButton::clicked,this,[this,choices]{const int row=choices->currentRow();auto n=node();auto a=n.value("choices").toArray();if(row<0||a.size()<=1){error->setText(tr("선택지는 하나 이상 필요합니다."));return;}const auto refs=externalReferences(n.value("id").toString(),a[row].toObject().value("id").toString());if(!refs.isEmpty()){error->setText(tr("선택지를 사용하는 시나리오가 있습니다. 참조를 먼저 수정하세요: ")+refs.join(", "));return;}a.removeAt(row);n["choices"]=a;setNode(n);fillProperties();});
    for(const auto &pair:QList<QPair<QString,QString>>{{"requirements","condition"},{"effects","effects"}}){auto b=new QPushButton(pair.first=="requirements"?tr("선택한 항목의 조건…"):tr("선택한 항목의 효과…"));b->setObjectName("scriptChoice-"+pair.first);form->addRow(b);connect(b,&QPushButton::clicked,this,[this,choices,pair]{int row=choices->currentRow();if(row<0){error->setText(tr("선택지 행을 먼저 선택하세요."));return;}auto n=node();auto a=n.value("choices").toArray();auto c=a[row].toObject();LogicEditor editor(c.value(pair.first),pair.second,context,this);if(editor.exec()==QDialog::Accepted){c[pair.first]=editor.value();a[row]=c;n["choices"]=a;setNode(n);}});}if(choices->rowCount())choices->setCurrentCell(0,1);
  }else if(type=="end")form->addRow(new QLabel(tr("이 노드에 도달하면 대본을 종료합니다.")));
  else form->addRow(new QLabel(tr("이 노드 종류는 편집하지 않고 원본 필드를 보존합니다.")));
  filling=false;
}

void ScriptAuthoringDialog::refreshDocumentIdentities() {
  if(documentIndex<0)return;
  const auto all=context.value("documents").toArray();
  QSignalBlocker blocker(documentField);
  for(int i=0;i<documents.size();++i){
    const auto doc=documents[i].toObject(), script=doc.value("data").toObject();
    const auto identity=scriptIdentity(doc,documents,all);
    documentField->setItemText(i,script.value("label").toString(script.value("id").toString()));
    documentField->setItemData(i,identity,identityRole);
    documentField->setItemData(i,identity+"\n"+doc.value("file").toString(),Qt::ToolTipRole);
  }
  if(auto summary=findChild<QLabel*>("scriptIdentitySummary"))summary->setText(documentField->itemData(documentIndex,identityRole).toString());
  refreshPredecessorSummary();
}
void ScriptAuthoringDialog::focusGraphNode(){
  const auto item=graphItems.value(node().value("id").toString());if(!item)return;
  graph->resetTransform();graph->centerOn(item->rect().center());syncGraphSelection();
}
void ScriptAuthoringDialog::syncGraphSelection(bool reveal){
  const auto id=node().value("id").toString();
  for(auto it=graphItems.begin();it!=graphItems.end();++it){
    const bool selected=it.key()==id;
    auto pen=it.value()->pen();pen.setColor(selected?QColor("#f3c779"):it.value()->data(1).value<QColor>());pen.setWidthF(selected?3:1.5);it.value()->setPen(pen);
  }
  if(reveal&&graphItems.contains(id))graph->ensureVisible(graphItems[id]->rect(),32,32);
  if(graphPosition)graphPosition->setText(tr("%1 · %2개 노드 · 확대 %3%").arg(nodeLabel(node())).arg(graphItems.size()).arg(qRound(graph->transform().m11()*100)));
  if(graphOverview)graphOverview->viewport()->update();
}
void ScriptAuthoringDialog::refreshGraph(){
  const auto center=graph->mapToScene(graph->viewport()->rect().center());
  QSignalBlocker blocker(graphScene);graphItems.clear();graphScene->clear();
  const auto d=data();const auto layout=layoutScriptGraph(d);QMap<QString,QJsonObject> nodes;
  for(auto value:d.value("nodes").toArray()){const auto n=value.toObject();nodes[n.value("id").toString()]=n;}
  int sequence=0;
  for(const auto &id:layout.order){
    const auto n=nodes[id];const auto rect=layout.rectangles[id];const auto type=n.value("type").toString();
    const bool unreachable=layout.unreachable.contains(id),start=id==d.value("entryNodeId").toString();
    const QColor border=unreachable?QColor("#e5a36d"):QColor("#668b9a");
    QColor fill=type=="choice"||type=="branch"?QColor("#423b55"):type=="scene"?QColor("#284848"):QColor("#253640");
    auto item=graphScene->addRect(rect,QPen(border,1.5,unreachable?Qt::DashLine:Qt::SolidLine),QBrush(fill));
    item->setFlag(QGraphicsItem::ItemIsSelectable);item->setData(0,id);item->setData(1,border);graphItems[id]=item;
    auto title=new QGraphicsTextItem(item);title->setPlainText(QString::number(++sequence)+"  "+(start?tr("시작 · "):QString())+nodeLabel(n).section(" · ",0,0));title->setDefaultTextColor(QColor("#e8c88e"));title->setPos(rect.topLeft()+QPointF(8,3));title->setAcceptedMouseButtons(Qt::NoButton);
    QString description=n.value("text").toString(n.value("prompt").toString());
    if(description.isEmpty())description=type=="scene"?tr("배경·인물·음악 연출"):type=="effect"?tr("%1개 효과 적용").arg(n.value("effects").toArray().size()):type=="branch"?tr("조건에 따라 참·거짓으로 나뉩니다"):type=="end"?tr("이야기 종료"):nodeLabel(n);
    const QString tooltip=nodeLabel(n)+"\n"+description+"\n"+id+(unreachable?"\n"+tr("시작 노드에서 연결되지 않음"):QString());item->setToolTip(tooltip);
    // Bound the card text while retaining full narration in the inspector/tooltip.
    auto body=new QGraphicsTextItem(item);body->setTextWidth(250);body->setPlainText(description.simplified().left(70)+(description.simplified().size()>70?QString("…"):QString()));body->setDefaultTextColor(QColor("#e5ecef"));body->setPos(rect.topLeft()+QPointF(8,27));body->setAcceptedMouseButtons(Qt::NoButton);
  }
  QMap<QString,QStringList> incomingCaptions;
  for(const auto &id:layout.order){
    const auto edges=labelledLinks(nodes[id]);int branch=0;
    for(const auto &connection:edges){
      const auto to=connection.first;if(!graphItems.contains(to))continue;
      const auto a=layout.rectangles[id],b=layout.rectangles[to];
      const QPointF p(a.left()+a.width()*(branch+1)/(edges.size()+1),a.bottom()),q(b.center().x(),b.top());
      QPainterPath line(p);line.cubicTo(p+QPointF(0,36),q-QPointF(0,36),q);
      const QColor color=edges.size()>1?QColor("#c3abd9"):QColor("#7cb4b3");auto edge=graphScene->addPath(line,QPen(color,2));edge->setZValue(-2);edge->setToolTip(connection.second);
      graphScene->addPolygon(QPolygonF{q,q+QPointF(-5,-9),q+QPointF(5,-9)},QPen(color),QBrush(color))->setZValue(-1);
      if(!connection.second.isEmpty())incomingCaptions[to] << QString::number(layout.order.indexOf(id)+1)+": "+connection.second;
      ++branch;
    }
  }
  // One caption per destination prevents converging choices from painting on
  // top of one another. Source numbers map back to the numbered graph cards.
  for(auto it=incomingCaptions.begin();it!=incomingCaptions.end();++it){
    const auto text=it.value().join(" / ");
    auto label=graphScene->addText(QFontMetrics(font()).elidedText(text,Qt::ElideRight,240));
    label->setDefaultTextColor(QColor("#eed9b4"));label->setToolTip(it.value().join('\n'));
    label->setData(2,"edge-caption");label->setData(3,it.key());
    const auto target=layout.rectangles[it.key()];
    label->setPos(target.center().x()-label->boundingRect().width()/2,target.top()-33);
    auto backing=graphScene->addRect(label->sceneBoundingRect().adjusted(-2,0,2,0),Qt::NoPen,QBrush(QColor("#17232a")));
    backing->setZValue(-1);
  }
  graphScene->setSceneRect(graphScene->itemsBoundingRect().adjusted(-24,-24,24,24));
  graph->centerOn(center);if(graphOverview)graphOverview->fitInView(graphScene->sceneRect(),Qt::KeepAspectRatio);
  syncGraphSelection();error->setText(scriptGraphIssues(d).join('\n'));
}

void ScriptAuthoringDialog::editPortraits(){
  auto originalScene=node().value("scene").toObject();auto values=originalScene.value("portraits").toArray();QJsonArray characters;for(auto v:context.value("documents").toArray())if(v.toObject().value("kind")=="character")characters.append(v.toObject().value("data"));
  QDialog dialog(this);dialog.setObjectName("scriptPortraitDialog");dialog.setWindowTitle(tr("장면의 인물 배치"));auto box=new QVBoxLayout(&dialog);auto help=new QLabel(tr("배율은 하단 중앙을 기준으로 적용합니다. 이동은 원래 인물 슬롯 크기의 비율입니다. 100% / 0% / 0%가 기본 배치입니다."));help->setWordWrap(true);box->addWidget(help);auto table=new QTableWidget(0,6);table->setObjectName("scriptPortraitRows");table->setHorizontalHeaderLabels({tr("인물"),tr("표정"),tr("위치"),tr("배율 %"),tr("가로 이동 %"),tr("세로 이동 %")});table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);box->addWidget(table);auto row=new QHBoxLayout;box->addLayout(row);auto add=button(row,"scriptPortraitAdd",tr("추가")),remove=button(row,"scriptPortraitRemove",tr("삭제"));auto error=new QLabel;box->addWidget(error);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);box->addWidget(buttons);
  auto fill=[&]{table->setRowCount(0);for(auto v:values){auto o=v.toObject();int r=table->rowCount();table->insertRow(r);auto person=new QComboBox;person->setProperty("original",o);for(auto c:characters)person->addItem(c.toObject().value("name").toString(),c.toObject().value("id").toString());auto id=o.value("characterId").toString();if(person->findData(id)<0)person->addItem(tr("없는 인물: ")+id,id);person->setCurrentIndex(person->findData(id));auto portrait=new QComboBox;auto fillPortrait=[=](QString selected){portrait->clear();for(auto c:characters)if(c.toObject().value("id").toString()==person->currentData().toString()){auto p=c.toObject().value("portraits").toObject();for(auto it=p.begin();it!=p.end();++it)portrait->addItem(it.key(),it.key());}if(!selected.isEmpty()&&portrait->findData(selected)<0)portrait->addItem(tr("없는 표정: ")+selected,selected);portrait->setCurrentIndex(selected.isEmpty()?0:portrait->findData(selected));};fillPortrait(o.value("portraitId").toString());connect(person,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[=]{fillPortrait({});});auto position=new QComboBox;for(auto s:{"left","center","right"})position->addItem(s,s);auto selected=o.value("position").toString();if(position->findData(selected)<0)position->addItem(selected,selected);position->setCurrentIndex(position->findData(selected));table->setCellWidget(r,0,person);table->setCellWidget(r,1,portrait);table->setCellWidget(r,2,position);for(int c=3;c<6;++c){const auto key=QStringList{"scale","offsetX","offsetY"}[c-3];auto value=new QDoubleSpinBox;value->setObjectName("scriptPortrait-"+key);value->setRange(c==3?10:-200,c==3?400:200);value->setDecimals(2);value->setSuffix("%");value->setValue(o.value(key).toDouble(c==3?1:0)*100);value->setProperty("baseline",value->value());table->setCellWidget(r,c,value);}}};
  auto collect=[&]{QJsonArray out;for(int r=0;r<table->rowCount();++r){auto person=qobject_cast<QComboBox*>(table->cellWidget(r,0));auto o=person->property("original").toJsonObject();o["characterId"]=person->currentData().toString();o["portraitId"]=qobject_cast<QComboBox*>(table->cellWidget(r,1))->currentData().toString();o["position"]=qobject_cast<QComboBox*>(table->cellWidget(r,2))->currentData().toString();for(int c=3;c<6;++c){auto field=qobject_cast<QDoubleSpinBox*>(table->cellWidget(r,c));if(field->value()!=field->property("baseline").toDouble())o[QStringList{"scale","offsetX","offsetY"}[c-3]]=field->value()/100;}out.append(o);}return out;};
  connect(add,&QPushButton::clicked,&dialog,[&]{values=collect();if(values.size()>=3){error->setText(tr("장면에는 최대 3명을 배치할 수 있습니다."));return;}QString id,portrait;if(!characters.empty()){auto c=characters.first().toObject();id=c.value("id").toString();auto p=c.value("portraits").toObject();if(!p.empty())portrait=p.begin().key();}values.append(QJsonObject{{"characterId",id},{"portraitId",portrait},{"position","center"}});fill();});connect(remove,&QPushButton::clicked,&dialog,[&]{auto r=table->currentRow();if(r<0)return;values=collect();values.removeAt(r);fill();});connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);connect(buttons,&QDialogButtonBox::accepted,&dialog,[&]{auto result=collect();for(auto v:result){auto o=v.toObject();bool found=false;for(auto c:characters)if(c.toObject().value("id")==o.value("characterId")&&c.toObject().value("portraits").toObject().contains(o.value("portraitId").toString()))found=true;if(!found){error->setText(tr("존재하는 인물과 표정을 선택하세요."));return;}}values=result;dialog.accept();});fill();dialog.resize(980,400);if(dialog.exec()==QDialog::Accepted){auto n=node();auto scene=n.value("scene").toObject();scene["portraits"]=values;n["scene"]=scene;setNode(n);}
}
void ScriptAuthoringDialog::previewScene(){
  if(!scenePreview||node().value("type")!="scene")return;
  QJsonObject project;for(auto document:context.value("documents").toArray())if(document.toObject().value("kind")=="project")project=document.toObject().value("data").toObject();
  if(project.value("visual").toObject().value("screens").toArray().isEmpty()){error->setText(tr("장면을 표시할 화면이 없습니다."));return;}
  const auto scene=node().value("scene").toObject();
  QDialog dialog(this);dialog.setObjectName("scriptScenePreviewDialog");dialog.setWindowTitle(tr("장면 미리보기 · 게임 진행 검증 아님"));dialog.resize(1120,820);
  auto layout=new QVBoxLayout(&dialog);auto note=new QLabel(tr("장면 미리보기 · 게임 진행 검증 아님\n초기 상태와 현재 장면 초안을 배포용 렌더러로 표시합니다. 일정·대본 진행·효과·저장을 실행하지 않습니다. 조건부 UI는 초기 능력과 상태를 기준으로 표시됩니다."));note->setWordWrap(true);layout->addWidget(note);
  auto screens=new QComboBox;screens->setObjectName("scriptPreviewScreen");for(auto value:project.value("visual").toObject().value("screens").toArray()){auto screen=value.toObject();screens->addItem(screen.value("label").toString(screen.value("id").toString()),screen.value("id").toString());}const auto story=screens->findData("story");if(story>=0)screens->setCurrentIndex(story);layout->addWidget(screens);
  auto renderer=new RendererPreviewDock(&dialog);renderer->setObjectName("scriptSceneRenderer");renderer->setFeatures(QDockWidget::NoDockWidgetFeatures);layout->addWidget(renderer,1);
  auto refreshContext=[&]{renderer->setContext(project,screens->currentData().toString(),{},context.value("revision").toInt(),context.value("root").toString(),{});};
  connect(screens,qOverload<int>(&QComboBox::currentIndexChanged),&dialog,[&]{refreshContext();});
  connect(renderer,&RendererPreviewDock::snapshotRequested,&dialog,[&,renderer]{
    if(saveGuard){const auto message=saveGuard();if(!message.isEmpty()){renderer->fail(message);return;}}
    QPointer<RendererPreviewDock> guard=renderer;
    scenePreview(scene,[guard](QJsonObject reply){if(!guard)return;if(!reply.value("error").toString().isEmpty()||!reply.contains("play")){guard->fail(reply.value("error").toString(QObject::tr("장면 미리보기를 준비하지 못했습니다.")));return;}guard->launch(reply.value("play").toObject());});
  });
  auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
  refreshContext();QTimer::singleShot(0,renderer,&RendererPreviewDock::begin);dialog.exec();
}
QJsonArray ScriptAuthoringDialog::edits()const{QJsonArray result;for(int i=0;i<documents.size();++i)if(documents[i]!=original[i]){auto d=documents[i].toObject();result.append(QJsonObject{{"type","replace"},{"file",d.value("file")},{"data",d.value("data")}});}for(int i=0;i<externalDocuments.size();++i)if(externalDocuments[i]!=externalOriginal[i]){auto d=externalDocuments[i].toObject();result.append(QJsonObject{{"type","replace"},{"file",d.value("file")},{"data",d.value("data")}});}return result;}
void ScriptAuthoringDialog::openConnections(){
  if(documentIndex<0||!commitId())return;
  auto snapshot=[this]{auto result=context;auto all=context.value("documents").toArray();
    for(int i=0;i<all.size();++i){auto file=all[i].toObject().value("file");for(auto v:documents)if(v.toObject().value("file")==file)all[i]=v;for(auto v:externalDocuments)if(v.toObject().value("file")==file)all[i]=v;}
    result["documents"]=all;return result;};
  StoryConnectionsDialog map(snapshot(),documents[documentIndex].toObject().value("file").toString(),this,
    [this,snapshot](QString file,QString nodeId,QString choiceId,QWidget *parent){
      selectDocument(file);auto d=data();QJsonValue requirements=d.value("requirements");auto nodes=d.value("nodes").toArray();int ni=-1,ci=-1;QJsonArray choices;
      if(!nodeId.isEmpty()){for(int i=0;i<nodes.size();++i)if(nodes[i].toObject().value("id")==nodeId)ni=i;
        if(ni<0)return snapshot();choices=nodes[ni].toObject().value("choices").toArray();for(int i=0;i<choices.size();++i)if(choices[i].toObject().value("id")==choiceId)ci=i;
        if(ci<0)return snapshot();requirements=choices[ci].toObject().value("requirements");}
      LogicEditor editor(requirements,"condition",snapshot(),parent);
      if(editor.exec()==QDialog::Accepted){if(ni<0)d["requirements"]=editor.value();else {auto c=choices[ci].toObject();c["requirements"]=editor.value();choices[ci]=c;auto n=nodes[ni].toObject();n["choices"]=choices;nodes[ni]=n;d["nodes"]=nodes;}setData(d);refreshMetadata();refreshNodes(nodeId);}
      return snapshot();});
  if(map.exec()==QDialog::Accepted){selectDocument(map.selectedFile);refreshNodes(map.selectedNode.isEmpty()?data().value("entryNodeId").toString():map.selectedNode);focusGraphNode();}
}
void StudioWindow::editScripts(){
  if(bridge.busy())return;
  const auto originalRevision=view.value("revision"),originalRoot=view.value("root");
  auto dialog=new ScriptAuthoringDialog(view,assets,this,[this,originalRevision,originalRoot]{
    if(view.value("revision")!=originalRevision||view.value("root")!=originalRoot)return tr("프로젝트가 변경되었습니다. 입력 내용을 덮어쓰지 않았습니다. 창을 다시 열어 확인하세요.");
    if(bridge.busy())return tr("다른 요청을 처리 중입니다. 완료 후 다시 저장하세요.");return QString{};
  },[this](QJsonObject scene,std::function<void(QJsonObject)> done){request("preview",{{"scene",scene}},std::move(done));});
  if(contentList&&contentList->currentItem())dialog->selectDocument(contentList->currentItem()->data(Qt::UserRole).toString());
  dialog->setApplyHandler([this](QJsonArray edits,std::function<void(QString)> done){if(edits.isEmpty()){done({});return;}request("edit",{{"edit",QJsonObject{{"type","batch"},{"edits",edits}}}},[done](QJsonObject reply){done(reply.value("error").toString());});});
  dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->open();
}
