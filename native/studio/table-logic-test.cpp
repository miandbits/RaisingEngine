#include "table-logic.hpp"
#include <QCoreApplication>
#include <iostream>

int main(int argc,char **argv) {
  QCoreApplication app(argc,argv);
  TableLogic::Context context({});context.attributes={"health","money"};context.attributeKinds={{"health","condition"},{"money","resource"}};context.flags={"met","promise"};context.items={"herb"};context.events={"arrival"};context.relations["friend/trust"]={{"kind","relationship"},{"characterId","friend"},{"relationshipId","trust"}};
  int checks=0;auto check=[&](bool value,const char *name){if(!value){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}++checks;};QString error;
  auto condition=context.condition("met=켜짐;health>=4|promise=켜짐;주차<=3",error);
  check(error.isEmpty()&&condition.value("op")=="any","OR groups");auto groups=condition.value("conditions").toArray();check(groups.size()==2&&groups[0].toObject().value("op")=="all","AND precedence");
  check(groups[1].toObject().value("conditions").toArray()[1].toObject().value("left").toObject().value("op")=="add","one-based week");
  context.condition("unknown>=1",error);check(!error.isEmpty(),"unknown reference rejected");error.clear();
  context.condition("met>=1",error);check(!error.isEmpty(),"boolean comparator rejected");error.clear();
  context.condition("health>=1|",error);check(!error.isEmpty(),"empty alternative rejected");error.clear();
  context.condition("health>=2147483648",error);check(!error.isEmpty(),"overflow rejected");error.clear();
  auto effects=context.effects("health=-4;friend/trust=2;item:herb=-1;met=켜짐;고정:health=70",error);
  check(error.isEmpty()&&effects.size()==5,"mixed effects");check(effects[1].toObject().value("target").toObject().value("relationshipId")=="trust","relationship target preserved");check(effects[4].toObject().value("op")=="set","assignment explicit");
  context.effects("health=1;missing=3",error);check(!error.isEmpty(),"bad effect prevents successful batch");error.clear();
  context.costs("money=4;money=2",error);check(!error.isEmpty(),"duplicate cost rejected");error.clear();
  context.costs("money=-1",error);check(!error.isEmpty(),"negative cost rejected");error.clear();
  check(context.costs("money=4;health=2",error).size()==2&&error.isEmpty(),"valid costs");
  check(context.condition("event:arrival",error).value("eventId")=="arrival","event completion condition");
  auto unseen=context.condition("!event:arrival",error);check(error.isEmpty()&&unseen.value("op")=="not"&&unseen.value("condition").toObject().value("eventId")=="arrival","unseen event condition");
  context.condition("!event:missing",error);check(!error.isEmpty(),"unseen event validates reference");error.clear();
  std::cout<<checks<<" table logic checks passed\n";
}
