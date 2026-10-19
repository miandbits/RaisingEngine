#include <QObject>
#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

// A deliberately small spreadsheet notation, compiled into the existing logic
// contract. No evaluation, scripting, project-specific names, or file access.
namespace TableLogic {
// Only call for enum cells, never names, IDs or dialogue content.
inline QString keyword(QString value) {
  static const QMap<QString,QString> aliases{
    {"on","켜짐"},{"off","꺼짐"},{"オン","켜짐"},{"オフ","꺼짐"},
    {"before activity","활동 전"},{"after activity","활동 후"},{"活動前","활동 전"},{"活動後","활동 후"},
    {"direct","직접"},{"直接","직접"},{"repeat visit","반복 방문"},{"repeat visits","반복 방문"},{"繰り返し訪問","반복 방문"},
    {"weekend","주말"},{"week end","주말"},{"週末","주말"},{"ending","결말"},{"エンディング","결말"},{"結末","결말"},
    {"dialogue","대사"},{"台詞","대사"},{"セリフ","대사"},{"会話","대사"},
    {"question","질문"},{"質問","질문"},{"choice","선택"},{"選択","선택"},
    {"branch","분기"},{"分岐","분기"},{"effect","효과"},{"効果","효과"},{"end","종료"},{"終了","종료"},
    {"week start","주 시작"},{"週開始","주 시작"},{"week finish","주 종료"},{"週終了","주 종료"},
    {"month start","월 시작"},{"月開始","월 시작"},{"month end","월 종료"},{"月終了","월 종료"}
  };
  return aliases.value(value.trimmed().toLower(),value);
}
inline bool id(const QString &value) { return value.size()<=64 && QRegularExpression("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$").match(value).hasMatch(); }
inline bool blank(const QString &value) { return value.isEmpty() || value=="-"; }
struct Context {
  QSet<QString> attributes, flags, characters, items, events;
  QMap<QString,QString> attributeKinds;
  QMap<QString,QJsonObject> relations;
  explicit Context(const QJsonArray &documents) {
    for(const auto &v:documents) {
      const auto doc=v.toObject(), data=doc.value("data").toObject();const auto kind=doc.value("kind").toString();
      if(kind=="attributes")for(const auto &a:data.value("attributes").toArray()){const auto attribute=a.toObject();const auto key=attribute.value("id").toString();attributes.insert(key);attributeKinds[key]=attribute.value("kind").toString();}
      if(kind=="flags")for(const auto &f:data.value("flags").toArray())flags.insert(f.toObject().value("id").toString());
      if(kind=="event")events.insert(data.value("id").toString());
      if(kind=="character") {
        const auto character=data.value("id").toString();characters.insert(character);
        for(const auto &r:data.value("relationships").toArray()) { const auto relation=r.toObject().value("id").toString();relations[character+"/"+relation]=QJsonObject{{"kind","relationship"},{"characterId",character},{"relationshipId",relation}}; }
      }
      if(kind=="project")for(const auto &item:data.value("gameplay").toObject().value("items").toArray())items.insert(item.toObject().value("id").toString());
    }
  }
  QJsonObject target(const QString &key) const {
    if(attributes.contains(key))return {{"kind","attribute"},{"attributeId",key}};
    if(relations.contains(key))return relations[key];
    if(key.startsWith("item:")&&items.contains(key.mid(5)))return {{"kind","inventory"},{"itemId",key.mid(5)}};
    return {};
  }
  QJsonObject condition(QString text,QString &error) const {
    text=text.trimmed();if(blank(text))return {{"op","always"}};
    for(const auto separator:{'|',';'})if(text.contains(separator)) {
      QJsonArray children;for(const auto &part:text.split(separator)){if(blank(part.trimmed())){error=QObject::tr("빈 조건이 있습니다.");return {};}auto child=condition(part,error);if(!error.isEmpty())return {};children.append(child);}
      return {{"op",separator=='|'?"any":"all"},{"conditions",children}};
    }
    if(text.startsWith("event:")||text.startsWith("!event:")) {const bool unseen=text.startsWith('!');const auto key=text.mid(unseen?7:6);if(!events.contains(key)){error=QObject::tr("알 수 없는 완료 사건: ")+key;return {};}const QJsonObject completed{{"op","eventCompleted"},{"eventId",key}};return unseen?QJsonObject{{"op","not"},{"condition",completed}}:completed;}
    const auto m=QRegularExpression("^([^<>=!]+)(>=|<=|!=|=|>|<)(.+)$").match(text);
    if(!m.hasMatch()){error=QObject::tr("조건은 대상>=값 또는 플래그=켜짐 형식입니다: ")+text;return {};}
    const auto key=m.captured(1).trimmed(),op=m.captured(2),operand=m.captured(3).trimmed();
    if(flags.contains(key)) {
      if(op!="=" || !QStringList{"켜짐","꺼짐","true","false"}.contains(operand)){error=QObject::tr("플래그 조건은 =켜짐 또는 =꺼짐을 사용하세요: ")+text;return {};}
      return {{"op","flag"},{"flagId",key},{"equals",operand=="켜짐"||operand=="true"}};
    }
    bool ok=false;auto n=operand.toInt(&ok);auto t=target(key);QJsonObject left;
    if(key=="주차"||key=="week"||key=="週")left={{"op","add"},{"left",QJsonObject{{"op","time"},{"field","periodIndex"}}},{"right",QJsonObject{{"op","literal"},{"value",1}}}};
    else if(!t.isEmpty())left={{"op","read"},{"target",t}};
    if(!ok||left.isEmpty()){error=QObject::tr("알 수 없는 대상 또는 정수가 아닌 비교값: ")+text;return {};}
    const QMap<QString,QString> comparators{{"=","eq"},{"!=","ne"},{">","gt"},{">=","gte"},{"<","lt"},{"<=","lte"}};
    return {{"op","compare"},{"comparator",comparators[op]},{"left",left},{"right",QJsonObject{{"op","literal"},{"value",n}}}};
  }
  QJsonArray effects(const QString &text,QString &error) const {
    QJsonArray result;if(blank(text))return result;
    for(const auto &entry:text.split(';')) {
      const auto pair=entry.split('=');if(pair.size()!=2){error=QObject::tr("효과는 대상=증감량 형식입니다: ")+entry;return {};}
      auto key=pair[0].trimmed();const auto operand=keyword(pair[1].trimmed());bool assign=key.startsWith("고정:")||key.startsWith("set:")||key.startsWith("設定:");if(assign)key=key.mid(key.indexOf(':')+1);
      if(flags.contains(key)&&!assign&&QStringList{"켜짐","꺼짐","true","false"}.contains(operand))result.append(QJsonObject{{"op","setFlag"},{"flagId",key},{"value",operand=="켜짐"||operand=="true"}});
      else {bool ok=false;const auto n=operand.toInt(&ok);const auto t=target(key);if(!ok||t.isEmpty()){error=QObject::tr("알 수 없는 효과 대상 또는 값: ")+entry;return {};}
        result.append(QJsonObject{{"op",assign?"set":"add"},{"target",t},{"value",QJsonObject{{"op","literal"},{"value",n}}}});
      }
    }return result;
  }
  QJsonArray costs(const QString &text,QString &error) const {
    QJsonArray result;QSet<QString> seen;if(blank(text))return result;
    for(const auto &entry:text.split(';')){const auto pair=entry.split('=');if(pair.size()!=2){error=QObject::tr("비용은 자원 ID=수량 형식입니다: ")+entry;return {};}
      const auto key=pair[0].trimmed();bool ok=false;const auto amount=pair[1].trimmed().toInt(&ok);
      if(!ok||amount<0||!attributeKinds.contains(key)||attributeKinds[key]=="stat"||seen.contains(key)){error=QObject::tr("비용의 자원·수량·중복을 확인하세요: ")+entry;return {};}
      seen.insert(key);result.append(QJsonObject{{"attributeId",key},{"amount",amount}});
    }return result;
  }
};
}
