#pragma once
#include <QTreeWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

// Static authoring references. Conditional consumers are deliberately not
// presented as a guaranteed runtime sequence.
inline QString workspaceRelation(const QString &relation) {
  const QMap<QString,QString> names{
    {"assignment",QObject::tr("인물·활동 배정")},{"condition",QObject::tr("조건에서 읽음")},{"cost",QObject::tr("비용")},
    {"effect",QObject::tr("효과로 변경")},{"branch",QObject::tr("행동 분기")},
    {"ui",QObject::tr("표시·행동 연결")},{"script",QObject::tr("대본")},
    {"choice",QObject::tr("선택지")},{"followup",QObject::tr("후속 사건 후보")},
    {"reference",QObject::tr("연결 대상")},{"reservation",QObject::tr("예약")},
    {"trigger",QObject::tr("실행 규칙")},{"initial",QObject::tr("초기값")},
    {"timing",QObject::tr("소요 시간")},{"consumer",QObject::tr("사용하거나 변경하는 곳")}};
  return names.value(relation,relation);
}

inline void fillWorkspaceConnections(QTreeWidget *tree,const QJsonArray &nodes,
                                     const QJsonArray &edges,const QString &key) {
  tree->clear();QMap<QString,QJsonObject> byKey;QMultiMap<QString,QJsonObject> outgoing,incoming;
  for(auto v:nodes)byKey[v.toObject().value("key").toString()]=v.toObject();
  for(auto v:edges){auto e=v.toObject();outgoing.insert(e.value("from").toString(),e);incoming.insert(e.value("to").toString(),e);}
  int count=0;
  std::function<void(QTreeWidgetItem*,QString,QSet<QString>,int)> walk;
  walk=[&](QTreeWidgetItem *parent,QString source,QSet<QString> visited,int depth){
    // Keep this a readable local trail, rather than unfolding the whole project.
    // Double-clicking any target makes it the new root for the next two levels.
    if(depth>1||count>=300)return;visited.insert(source);
    auto refs=outgoing.values(source);const auto kind=byKey.value(source).value("kind").toString();
    // An effect/appointment may unlock consumers through a shared flag, location
    // or activity. Show those dependencies without evaluating or inventing state.
    if(QStringList{"flag","activity","location"}.contains(kind))
      for(auto e:incoming.values(source))if(e.value("from").toString().startsWith("event:")&&
          (e.value("relation")=="condition"||e.value("path")=="/activityId"||e.value("path")=="/locationId")){
        e["to"]=e.value("from");e["relation"]="followup";e["resolved"]=byKey.contains(e.value("to").toString());refs.append(e);
      }
    if(QStringList{"attribute","relationship","item","stateGroup","stateValue","userField"}.contains(kind))
      for(auto e:incoming.values(source)){
        const auto from=e.value("from").toString();
        if(from.startsWith("screen:")||from.startsWith("actor:")||visited.contains(from))continue;
        e["to"]=from;e["relation"]="consumer";e["resolved"]=byKey.contains(from);refs.append(e);
      }
    QSet<QString> shown;
    QList<QPair<QTreeWidgetItem*,QString>> expand;
    for(auto e:refs){
      if(count>=300)break;
      const auto target=e.value("to").toString(),relation=e.value("relation").toString();
      if(shown.contains(target+"|"+relation))continue;shown.insert(target+"|"+relation);
      auto label=byKey.value(target).value("label").toString(target);const bool resolved=e.value("resolved").toBool()&&byKey.contains(target);
      if(!resolved)label+=QObject::tr(" · 없는/중복 참조");
      const bool loop=visited.contains(target);if(loop)label+=QObject::tr(" · 이미 표시됨");
      auto row=parent?new QTreeWidgetItem(parent):new QTreeWidgetItem(tree);
      row->setText(0,workspaceRelation(relation));row->setText(1,label.simplified().left(100));
      row->setToolTip(1,label+"\n"+e.value("file").toString()+e.value("path").toString());
      row->setData(0,Qt::UserRole,resolved?target:QString());row->setData(0,Qt::UserRole+1,e);
      if(!resolved)row->setForeground(1,QColor("#d47762"));
      ++count;row->setExpanded(false);
      if(resolved&&!loop)expand.append({row,target});
    }
    for(const auto &entry:expand)walk(entry.first,entry.second,visited,depth+1);
  };
  walk(nullptr,key,{},0);
  if(count==0)new QTreeWidgetItem(tree,{QObject::tr("연결 없음"),QObject::tr("가운데에서 데이터·행동·조건을 연결하세요.")});
  if(count>=300)new QTreeWidgetItem(tree,{QObject::tr("표시 한도 300개"),QObject::tr("대상을 열면 그 항목부터 다시 따라갑니다.")});
}
