#include "relationship-editor.hpp"
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTableWidget>
#include <QVBoxLayout>
#include <limits>
#include <functional>

RelationshipEditor::RelationshipEditor(QJsonObject source, QJsonObject ctx, QWidget *parent)
    : QWidget(parent), character(source), context(ctx) {
  auto box = new QVBoxLayout(this);
  auto help = new QLabel(tr("이 인물과의 신뢰·호감 등 관계 수치를 만듭니다. 활동과 선택지의 효과에서 인물 / 관계 이름을 골라 사용하세요. 기존 ID는 연결을 보존하며, 표시 이름은 자유롭게 바꿀 수 있습니다."));
  help->setWordWrap(true); box->addWidget(help);
  table = new QTableWidget(0, 6); table->setObjectName("relationshipTable");
  table->setHorizontalHeaderLabels({tr("ID"), tr("표시 이름"), tr("설명"), tr("최소"), tr("초기값"), tr("최대")});
  table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); box->addWidget(table);
  for (auto value : character.value("relationships").toArray()) append(value.toObject(), false);
  auto row = new QHBoxLayout; box->addLayout(row);
  auto add = new QPushButton(tr("관계 수치 추가")); add->setObjectName("relationshipAdd"); row->addWidget(add);
  auto remove = new QPushButton(tr("선택 관계 삭제")); remove->setObjectName("relationshipRemove"); row->addWidget(remove);
  connect(add, &QPushButton::clicked, this, [this] {
    QSet<QString> used; for (int i = 0; i < table->rowCount(); ++i) used.insert(table->item(i, 0)->text());
    int n = 1; while (used.contains(QString("relationship-%1").arg(n))) ++n;
    append({{"id", QString("relationship-%1").arg(n)}, {"label", tr("새 관계")}, {"min", 0}, {"initial", 0}, {"max", 100}}, true);
  });
  connect(remove, &QPushButton::clicked, this, [this] { if (table->currentRow() >= 0) table->removeRow(table->currentRow()); });
}
void RelationshipEditor::append(QJsonObject data, bool fresh) {
  const int row = table->rowCount(); table->insertRow(row);
  const QStringList keys{"id", "label", "description"};
  for (int c = 0; c < 3; ++c) table->setItem(row, c, new QTableWidgetItem(data.value(keys[c]).toString()));
  table->item(row, 0)->setData(Qt::UserRole, data);
  if (!fresh) table->item(row, 0)->setFlags(table->item(row, 0)->flags() & ~Qt::ItemIsEditable);
  const QStringList ranges{"min", "initial", "max"};
  for (int c = 0; c < 3; ++c) {
    // Keep displayed, accessible and serialized values in one table model.
    // A cell widget over an empty item lets accessibility edit a hidden value.
    table->setItem(row, c + 3, new QTableWidgetItem(QString::number(data.value(ranges[c]).toInt())));
  }
  table->setCurrentCell(row, 1);
}
QJsonArray RelationshipEditor::value(QString *error) const {
  QJsonArray values; QSet<QString> ids;
  const QRegularExpression pattern("^[a-z][a-z0-9]*(?:-[a-z0-9]+)*$");
  for (int r = 0; r < table->rowCount(); ++r) {
    auto data = table->item(r, 0)->data(Qt::UserRole).toJsonObject();
    const auto id = table->item(r, 0)->text().trimmed(), label = table->item(r, 1)->text().trimmed();
    if (id.size() > 64 || !pattern.match(id).hasMatch() || ids.contains(id) || label.isEmpty()) {
      *error = tr("관계 %1행: 중복 없는 영문 소문자·숫자·하이픈 ID와 표시 이름이 필요합니다.").arg(r + 1); return {};
    }
    ids.insert(id); data["id"] = id; data["label"] = label;
    const auto description = table->item(r, 2)->text();
    if (!description.isEmpty() || data.contains("description")) data["description"] = description;
    const QStringList keys{"min", "initial", "max"};
    for (int c = 0; c < 3; ++c) {
      bool valid = false;
      const auto number = table->item(r, c + 3)->text().trimmed().toLongLong(&valid);
      if (!valid || number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
        *error = tr("관계 %1: 최소·초기값·최대에는 32비트 정수를 입력하세요.").arg(label); return {};
      }
      data[keys[c]] = int(number);
    }
    if (data["min"].toInt() > data["initial"].toInt() || data["initial"].toInt() > data["max"].toInt()) {
      *error = tr("관계 %1: 최소 ≤ 초기값 ≤ 최대여야 합니다.").arg(label); return {};
    }
    values.append(data);
  }
  for (auto old : character.value("relationships").toArray()) {
    const auto id = old.toObject().value("id").toString(), owner = character.value("id").toString();
    if (ids.contains(id)) continue;
    std::function<bool(QJsonValue)> used = [&](QJsonValue v) {
      if (v.isArray()) { for (auto child : v.toArray()) if (used(child)) return true; }
      if (v.isObject()) {
        const auto obj = v.toObject();
        if (obj.value("characterId") == owner && obj.value("relationshipId") == id) return true;
        if (obj.value("relationships").toObject().value(owner).toObject().contains(id)) return true;
        for (auto it = obj.begin(); it != obj.end(); ++it) if (used(it.value())) return true;
      }
      return false;
    };
    for (auto doc : context.value("documents").toArray()) if (used(doc.toObject().value("data"))) {
      *error = tr("관계 '%1'은 %2에서 사용 중입니다. 해당 조건·효과의 대상을 바꾼 뒤 삭제하세요.").arg(id, doc.toObject().value("file").toString()); return {};
    }
  }
  return values;
}
