#pragma once
#include <QStringList>

inline QStringList battleHudFields(const QString &source) {
  if(source=="battleActors")return {"id","name","side","hp","maxHp","hpText","battleTarget","value","maxValue"};
  if(source=="battleActions")return {"id","name","displayName","battleAction","itemId","target","selected","enabled","reason","action"};
  if(source=="battleTargets")return {"id","name","displayName","battleTarget","selected","enabled","reason","action"};
  if(source=="battleLog")return {"id","text","turn","battleAction","heroDamage","enemyDamage","healed","outcome"};
  if(source=="battleRewards")return {"id","name","amount","discarded","text","kind"};
  return {};
}
