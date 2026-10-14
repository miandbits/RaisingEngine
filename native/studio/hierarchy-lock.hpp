#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

// Placement locks apply to the complete ancestor chain, independent of the
// canvas's flattened preview data. The owner is useful for actionable feedback.
inline QString elementLockOwner(const QJsonArray& elements,QString id,bool includeSelf=true) {
  QSet<QString> visited;bool first=true;
  while(!id.isEmpty()&&!visited.contains(id)) {
    visited.insert(id);QJsonObject element;
    for(const auto value:elements)if(value.toObject().value("id")==id){element=value.toObject();break;}
    if(element.empty())return {};
    if((includeSelf||!first)&&element.value("locked").toBool())return id;
    first=false;id=element.value("parentId").toString();
  }
  return {};
}
