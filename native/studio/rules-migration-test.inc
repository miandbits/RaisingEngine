#include "rules-migration.hpp"

void StudioTests::genericRulesMigration() {
  const auto parsed = [](const char *json) { return QJsonDocument::fromJson(json).object(); };
  const auto original = parsed(R"JSON({
    "gameplay": {
      "actions": [{"id":"service-heal","name":"Existing","costs":[],"effects":[]}],
      "policyGroups": [{"id":"daily-policy","name":"Existing group","trigger":{"type":"afterActivity"},"options":[]}],
      "services": [{"id":"heal","name":"Healing","locationId":"temple","costs":[{"attributeId":"money","amount":4}],"effects":[],"maxQuantity":3,"confirmation":{"title":"Proceed?"}}],
      "diets": [{"id":"normal","name":"Normal","locationId":"home","intervalPeriods":3,"costs":[],"effects":[]},{"id":"light","name":"Light","locationId":"home","intervalPeriods":3,"costs":[],"effects":[]}]
    },
    "visual": {"screens":[{"id":"home","elements":[
      {"id":"services","kind":"services"},{"id":"diets","kind":"diets"},
      {"id":"heal","kind":"button","action":"useService","serviceId":"heal","quantity":2},
      {"id":"select","kind":"button","action":"setDiet","dietId":"normal"},
      {"id":"clear","kind":"button","action":"setDiet"},
      {"id":"instance","kind":"component","componentId":"card","overrides":[{"elementId":"child","values":{"serviceId":"heal"}}]},
      {"id":"policy-instance","kind":"component","componentId":"policy-card","overrides":[{"elementId":"choice","values":{"action":"setDiet"}}]}
    ]}],"components":[{"id":"card","elements":[{"id":"child","kind":"button","action":"useService","serviceId":"heal"}]},{"id":"policy-card","elements":[{"id":"choice","kind":"button","action":"setDiet","dietId":"normal"}]}]}
  })JSON");
  QString error;
  const auto migrated = migrateFacilityRules(original, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  QVERIFY(original.value("gameplay").toObject().contains("diets"));
  const auto gameplay = migrated.value("gameplay").toObject();
  QVERIFY(!gameplay.contains("diets")); QVERIFY(!gameplay.contains("services"));
  const auto actions = gameplay.value("actions").toArray();
  QCOMPARE(actions.size(), 4);
  const auto service = actions[1].toObject();
  QCOMPARE(service.value("id").toString(), QStringLiteral("service-heal-2"));
  QCOMPARE(service.value("locationIds").toArray(), QJsonArray{QStringLiteral("temple")});
  QVERIFY(!service.contains("locationId")); QVERIFY(!service.contains("requirements"));
  QCOMPARE(service.value("maxQuantity").toInt(), 3); QVERIFY(service.value("rejectNoEffect").toBool());
  QCOMPARE(service.value("costs"), original.value("gameplay").toObject().value("services").toArray()[0].toObject().value("costs"));
  QCOMPARE(service.value("confirmation").toObject().value("title").toString(), QStringLiteral("Proceed?"));
  const auto policy = gameplay.value("policyGroups").toArray()[1].toObject();
  QCOMPARE(policy.value("id").toString(), QStringLiteral("daily-policy-2"));
  QCOMPARE(policy.value("trigger").toObject().value("intervalPeriods").toInt(), 3);
  QCOMPARE(policy.value("options").toArray()[0].toObject().value("actionId").toString(), QStringLiteral("policy-normal"));
  QVERIFY(!actions[2].toObject().contains("locationId")); QVERIFY(!actions[2].toObject().contains("locationIds"));
  QVERIFY(!actions[2].toObject().contains("intervalPeriods"));
  const auto elements = migrated.value("visual").toObject().value("screens").toArray()[0].toObject().value("elements").toArray();
  QCOMPARE(elements[0].toObject().value("kind").toString(), QStringLiteral("actions"));
  QCOMPARE(elements[0].toObject().value("actionIds").toArray(), QJsonArray{QStringLiteral("service-heal-2")});
  QCOMPARE(elements[1].toObject().value("groupIds").toArray(), QJsonArray{QStringLiteral("daily-policy-2")});
  QCOMPARE(elements[2].toObject().value("action").toString(), QStringLiteral("executeAction"));
  QCOMPARE(elements[2].toObject().value("actionId").toString(), QStringLiteral("service-heal-2"));
  QCOMPARE(elements[2].toObject().value("quantity").toInt(), 2); QVERIFY(!elements[2].toObject().contains("serviceId"));
  QCOMPARE(elements[3].toObject().value("optionId").toString(), QStringLiteral("normal"));
  QCOMPARE(elements[3].toObject().value("action").toString(), QStringLiteral("selectPolicy"));
  QVERIFY(elements[4].toObject().value("optionId").isNull());
  const auto serviceOverride = elements[5].toObject().value("overrides").toArray()[0].toObject().value("values").toObject();
  QCOMPARE(serviceOverride.value("actionId").toString(), QStringLiteral("service-heal-2")); QVERIFY(!serviceOverride.contains("serviceId"));
  const auto policyOverride = elements[6].toObject().value("overrides").toArray()[0].toObject().value("values").toObject();
  QCOMPARE(policyOverride.value("action").toString(), QStringLiteral("selectPolicy")); QVERIFY(!policyOverride.contains("optionId")); // Inherit the converted component selection.
  const auto componentChild = migrated.value("visual").toObject().value("components").toArray()[0].toObject().value("elements").toArray()[0].toObject();
  QCOMPARE(componentChild.value("action").toString(), QStringLiteral("executeAction"));
  QCOMPARE(componentChild.value("actionId").toString(), QStringLiteral("service-heal-2"));
  QCOMPARE(migrateFacilityRules(migrated, &error), migrated); QVERIFY(error.isEmpty());
  for (const auto &field : {QStringLiteral("locationId"), QStringLiteral("intervalPeriods")}) {
    auto invalid = original, data = invalid.value("gameplay").toObject();
    auto diets = data.value("diets").toArray(); auto changed = diets[1].toObject();
    changed[field] = field == "locationId" ? QJsonValue("elsewhere") : QJsonValue(1);
    diets[1] = changed; data["diets"] = diets; invalid["gameplay"] = data;
    QCOMPARE(migrateFacilityRules(invalid, &error), invalid); QVERIFY(!error.isEmpty());
  }
  const auto empty = parsed(R"JSON({"gameplay":{"locations":[]},"visual":{"screens":[]}})JSON");
  QCOMPARE(migrateFacilityRules(empty, &error), empty); QVERIFY(error.isEmpty());
  const auto serviceOnly = parsed(R"JSON({"gameplay":{"services":[{"id":"heal","name":"Heal","locationId":"home","costs":[],"effects":[]}]}})JSON");
  const auto convertedService = migrateFacilityRules(serviceOnly, &error);
  QVERIFY(error.isEmpty()); QVERIFY(!convertedService.contains("visual"));
  QVERIFY(!convertedService.value("gameplay").toObject().contains("policyGroups"));
  QVERIFY(!convertedService.value("gameplay").toObject().value("actions").toArray()[0].toObject().contains("requirements"));
  auto missingReference = original, visual = missingReference.value("visual").toObject();
  visual["screens"] = QJsonArray{parsed(R"JSON({"id":"home","elements":[{"kind":"button","action":"useService","serviceId":"missing"}]})JSON")};
  missingReference["visual"] = visual;
  QCOMPARE(migrateFacilityRules(missingReference, &error), missingReference); QVERIFY(!error.isEmpty());
  auto emptyServiceWidget = original, emptyServicesData = emptyServiceWidget.value("gameplay").toObject();
  emptyServicesData.remove("services"); emptyServiceWidget["gameplay"] = emptyServicesData;
  QCOMPARE(migrateFacilityRules(emptyServiceWidget, &error), emptyServiceWidget); QVERIFY(!error.isEmpty());
  auto longIds = serviceOnly, longData = longIds.value("gameplay").toObject();
  auto longServices = longData.value("services").toArray(); auto longService = longServices[0].toObject();
  longService["id"] = QString(64, QChar('a')); longServices[0] = longService;
  longData["services"] = longServices;
  const auto occupied = QStringLiteral("service-") + QString(56, QChar('a'));
  longData["actions"] = QJsonArray{QJsonObject{{"id", occupied}}}; longIds["gameplay"] = longData;
  const auto generated = migrateFacilityRules(longIds, &error).value("gameplay").toObject().value("actions").toArray()[1].toObject().value("id").toString();
  QVERIFY(error.isEmpty()); QVERIFY(generated.size() <= 64); QVERIFY(generated != occupied); QVERIFY(generated.endsWith("-2"));
}
