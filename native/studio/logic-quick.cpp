#include <QObject>
#include "logic-editor.hpp"
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <limits>

QWidget *LogicEditor::quickEditor() {
  auto group = new QGroupBox(tr("간단한 수치 규칙 — 대상과 값을 골라 추가하세요"));
  group->setObjectName("logicQuickEditor");
  auto layout = new QVBoxLayout(group);
  auto row = new QHBoxLayout;
  auto search = new QLineEdit; search->setObjectName("quickTargetSearch");
  search->setPlaceholderText(tr("능력·자원·관계·소지품 이름 또는 ID 검색"));
  layout->addWidget(search);
  auto owner = new QComboBox; owner->setObjectName("quickOwner");
  owner->addItem(tr("공유 / 기존 대상"), QJsonObject{});
  owner->addItem(tr("행동 실행자"), QJsonObject{{"kind","actor"}});
  for (const auto &doc : context.value("documents").toArray()) if (doc.toObject().value("kind") == "project")
    for (const auto &member : doc.toObject().value("data").toObject().value("roster").toObject().value("members").toArray()) {
      const auto id = member.toObject().value("characterId").toString();
      QString name = id;
      for (const auto &character : references("character")) if (character.toObject().value("id") == id) name = character.toObject().value("name").toString(id);
      owner->addItem(tr("지정 · %1 [%2]").arg(name, id), QJsonObject{{"kind","character"},{"characterId",id}});
    }
  row->addWidget(owner);
  auto target = new QComboBox; target->setObjectName("quickTarget"); target->setMinimumWidth(230); row->addWidget(target, 1);
  QList<QPair<QString,QJsonObject>> targets;
  for (const auto &kind : {QString("attribute"), QString("inventory")}) for (const auto &value : references(kind)) {
    const auto data = value.toObject(); const auto id = data.value("id").toString();
    const auto name = data.value("label").toString(data.value("name").toString(id));
    targets.append({(kind == "attribute" ? tr("능력·자원 · ") : tr("소지품 · ")) + name + " [" + id + "]",
                    QJsonObject{{"kind",kind},{kind == "attribute" ? "attributeId" : "itemId",id}}});
  }
  for (const auto &character : references("character")) {
    const auto data = character.toObject(); const auto id = data.value("id").toString();
    for (const auto &relation : references("relationship", id)) {
      const auto r = relation.toObject(); const auto rid = r.value("id").toString();
      targets.append({tr("관계 · %1 / %2 [%3/%4]").arg(data.value("name").toString(id),r.value("label").toString(rid),id,rid),
                      QJsonObject{{"kind","relationship"},{"characterId",id},{"relationshipId",rid}}});
    }
  }
  auto operation = new QComboBox; operation->setObjectName("quickOperation"); row->addWidget(operation);
  const bool effects = rootKind == "effects";
  if (effects) { operation->addItem(tr("증감"),"add"); operation->addItem(tr("값 지정"),"set"); }
  else { for (const auto &pair : QList<QPair<QString,QString>>{{"gte",QObject::tr("이상 ≥")},{"lte",QObject::tr("이하 ≤")},{"eq",QObject::tr("같음 =")},{"gt",QObject::tr("초과 >")},{"lt",QObject::tr("미만 <")},{"ne",QObject::tr("다름 ≠")}}) operation->addItem(pair.second,pair.first); }
  auto amount = new QSpinBox; amount->setObjectName("quickAmount"); amount->setRange(std::numeric_limits<int>::min(),std::numeric_limits<int>::max()); row->addWidget(amount);
  layout->addLayout(row);
  auto help = new QLabel(effects ? tr("음수는 감소입니다. 추가를 눌러야 반영되며, 기존 효과는 그대로 보존합니다.") : tr("선택 조건만 교체하거나 기존 전체 조건에 AND로 추가합니다. 아래 트리에서 결과를 확인한 후 저장하세요.")); help->setWordWrap(true); layout->addWidget(help);
  auto actions = new QHBoxLayout;
  auto apply = new QPushButton(effects ? tr("효과 추가") : tr("선택 조건 교체")); apply->setObjectName("quickApply"); actions->addWidget(apply);
  auto append = new QPushButton(tr("AND 조건 추가")); append->setObjectName("quickAppend"); append->setVisible(!effects); actions->addWidget(append); layout->addLayout(actions);
  const auto fill = [=] {
    const auto previous = target->currentData(); target->clear();
    for (const auto &entry : targets) if (entry.first.contains(search->text(),Qt::CaseInsensitive)) target->addItem(entry.first,entry.second);
    for (int i=0;i<target->count();++i) if (target->itemData(i)==previous) target->setCurrentIndex(i);
    apply->setEnabled(target->count()>0); append->setEnabled(target->count()>0);
  };
  connect(search,&QLineEdit::textChanged,group,fill); fill();
  auto rule = [=] {
    auto selected = target->currentData().toJsonObject();
    if (!owner->currentData().toJsonObject().empty()) selected["owner"] = owner->currentData().toJsonObject();
    const QJsonObject value{{"op","literal"},{"value",amount->value()}};
    return effects ? QJsonObject{{"op",operation->currentData().toString()},{"target",selected},{"value",value}}
                   : QJsonObject{{"op","compare"},{"comparator",operation->currentData().toString()},{"left",QJsonObject{{"op","read"},{"target",selected}}},{"right",value}};
  };
  connect(apply,&QPushButton::clicked,this,[=] {
    if (target->currentIndex()<0) return;
    if (effects) { auto entries=root.toArray(); entries.append(rule()); update({},entries); }
    else if (selectedKind=="condition") update(selectedPath,rule());
    else error->setText(tr("아래 트리에서 교체할 조건을 선택하세요. 값·대상 항목은 조건으로 교체할 수 없습니다."));
  });
  connect(append,&QPushButton::clicked,this,[=] {
    if (target->currentIndex()<0) return;
    if (root.toObject().value("op")=="always") update({},rule());
    else if (root.toObject().value("op")=="all") { auto object=root.toObject();auto conditions=object.value("conditions").toArray();conditions.append(rule());object["conditions"]=conditions;update({},object); }
    else update({},QJsonObject{{"op","all"},{"conditions",QJsonArray{root,rule()}}});
  });
  return group;
}
