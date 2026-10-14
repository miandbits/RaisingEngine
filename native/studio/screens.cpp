#include "studio.hpp"
#include <QInputDialog>
#include <QMessageBox>
#include <QSet>

namespace {
QString askName(QWidget *parent, const QString &title, const QString &initial) {
  QInputDialog dialog(parent);
  dialog.setObjectName("screenNameDialog");
  dialog.setWindowTitle(title);
  dialog.setLabelText(QObject::tr(
      "화면 이름 (1–100자)\n연결에 쓰는 ID는 자동으로 관리됩니다."));
  dialog.setTextValue(initial);
  dialog.setOkButtonText(QObject::tr("확인"));
  dialog.setCancelButtonText(QObject::tr("취소"));
  auto field = dialog.findChild<QLineEdit *>();
  field->setMaxLength(100);
  while (dialog.exec() == QDialog::Accepted) {
    auto name = dialog.textValue().trimmed();
    if (!name.isEmpty())
      return name;
    dialog.setLabelText(
        QObject::tr("공백이 아닌 화면 이름을 입력하세요. (1–100자)"));
  }
  return {};
}

// Screen IDs are references, while labels are free to change. Rewire only
// self-navigation in a copy; links to other screens keep their destinations.
QJsonValue copyReferences(QJsonValue value, const QString &from,
                          const QString &to) {
  if (value.isArray()) {
    QJsonArray array;
    for (auto child : value.toArray())
      array.append(copyReferences(child, from, to));
    return array;
  }
  if (value.isObject()) {
    auto object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it)
      it.value() = it.key() == "screenId" && it.value() == from
                       ? QJsonValue(to)
                       : copyReferences(it.value(), from, to);
    return object;
  }
  return value;
}

void references(QJsonValue value, const QString &id, const QString &path,
                QStringList &found) {
  if (value.isArray()) {
    auto array = value.toArray();
    for (int i = 0; i < array.size(); ++i)
      references(array[i], id, path + "/" + QString::number(i), found);
  } else if (value.isObject()) {
    auto object = value.toObject();
    for (auto it = object.begin(); it != object.end(); ++it) {
      const auto next = path + "/" + it.key();
      if (it.key() == "screenId" && it.value() == id)
        found.append(next);
      else
        references(it.value(), id, next, found);
    }
  }
}
} // namespace

QString StudioWindow::managedScreenId() const {
  auto item = projectList->currentItem();
  return item ? item->data(Qt::UserRole).toString() : screenId;
}

void StudioWindow::updateScreenActions() {
  const bool loaded = !projectData().isEmpty() && !bridge.busy();
  const bool selected = loaded && !managedScreenId().isEmpty();
  newScreenAction->setEnabled(loaded);
  duplicateScreenAction->setEnabled(selected);
  renameScreenAction->setEnabled(selected);
  deleteScreenAction->setEnabled(selected);
}

void StudioWindow::openScreen(const QString &id) {
  if (bridge.busy())
    return;
  for (auto s : projectData()["visual"].toObject()["screens"].toArray()) {
    if (s.toObject()["id"].toString() != id)
      continue;
    if (!openScreens.contains(id))
      openScreens.append(id);
    screenId = id;
    selection.clear();
    refresh(true);
    return;
  }
}

void StudioWindow::closeScreen(int index) {
  if (bridge.busy() || index < 0 || index >= documents->count())
    return;
  const auto id = documents->tabData(index).toString();
  openScreens.removeAll(id);
  if (screenId == id) {
    screenId = openScreens.isEmpty()
                   ? QString()
                   : openScreens[qMin(index, int(openScreens.size()) - 1)];
    selection.clear();
  }
  refresh(true);
}

void StudioWindow::createScreen(bool duplicate) {
  if (bridge.busy() || projectData().isEmpty())
    return;
  auto project = projectData();
  auto visual = project["visual"].toObject();
  auto screens = visual["screens"].toArray();
  if (screens.size() >= 100) {
    QMessageBox::information(
        this, tr("화면 한도"),
        tr("현재 데이터 계약은 프로젝트당 100개 화면을 지원합니다."));
    return;
  }
  QJsonObject source;
  QSet<QString> ids;
  for (auto s : screens) {
    auto screen = s.toObject();
    ids.insert(screen["id"].toString());
    if (screen["id"].toString() == managedScreenId())
      source = screen;
  }
  if (duplicate && source.isEmpty())
    return;
  auto name =
      askName(this, duplicate ? tr("화면 복제") : tr("새 화면 만들기"),
              duplicate ? source["label"].toString().left(97) + tr(" 복사")
                        : tr("새 화면"));
  if (name.isEmpty())
    return;
  const auto base =
      duplicate ? source["id"].toString().left(40) + "-copy-" : "screen-";
  int n = 1;
  while (ids.contains(base + QString::number(n)))
    ++n;
  const auto id = base + QString::number(n);
  auto screen =
      duplicate ? copyReferences(source, source["id"].toString(), id).toObject()
                : QJsonObject{{"elements", QJsonArray{}}};
  screen["id"] = id;
  screen["label"] = name;
  screens.append(screen);
  visual["screens"] = screens;
  project["visual"] = visual;
  request("edit",
          {{"edit", QJsonObject{{"type", "replace"},
                                {"file", projectFile},
                                {"data", project}}}},
          [this, id](QJsonObject reply) {
            if (reply["error"].toString().isEmpty())
              openScreen(id);
          });
}

void StudioWindow::renameScreen() {
  if (bridge.busy())
    return;
  const auto id = managedScreenId();
  auto project = projectData();
  auto visual = project["visual"].toObject();
  auto screens = visual["screens"].toArray();
  for (int i = 0; i < screens.size(); ++i) {
    auto screen = screens[i].toObject();
    if (screen["id"].toString() != id)
      continue;
    auto name = askName(this, tr("화면 이름 변경"), screen["label"].toString());
    if (name.isEmpty() || name == screen["label"].toString())
      return;
    screen["label"] = name;
    screens[i] = screen;
    visual["screens"] = screens;
    project["visual"] = visual;
    replaceProject(project);
    return;
  }
}

void StudioWindow::deleteScreen() {
  if (bridge.busy())
    return;
  const auto id = managedScreenId();
  auto project = projectData();
  auto visual = project["visual"].toObject();
  auto screens = visual["screens"].toArray();
  QJsonArray kept;
  QJsonObject removed;
  int removedIndex = -1;
  for (int i = 0; i < screens.size(); ++i) {
    auto s = screens[i];
    if (s.toObject()["id"].toString() == id) {
      removed = s.toObject();
      removedIndex = i;
    } else
      kept.append(s);
  }
  if (removed.isEmpty())
    return;
  if (QStringList{"title", "schedule", "story", "ending"}.contains(id)) {
    QMessageBox::information(this, tr("필수 화면"),
                             tr("‘%1’은 현재 런타임의 필수 화면입니다. 삭제할 "
                                "수 없지만 이름과 내용은 편집할 수 있습니다.")
                                 .arg(removed["label"].toString()));
    return;
  }
  QStringList found;
  // Keep original indices in diagnostic paths. Self-links inside the removed
  // screen disappear with it and must not block deletion.
  for (auto d : view["documents"].toArray()) {
    auto doc = d.toObject();
    auto data = doc["data"];
    if (doc["file"].toString() == projectFile) {
      auto p = data.toObject();
      auto v = p["visual"].toObject();
      auto rows = v["screens"].toArray();
      rows[removedIndex] = QJsonValue();
      v["screens"] = rows;
      p["visual"] = v;
      data = p;
    }
    references(data, id, doc["file"].toString() + "#", found);
  }
  if (!found.isEmpty()) {
    QMessageBox box(QMessageBox::Warning,
                    tr("연결된 화면은 삭제할 수 없습니다"),
                    tr("‘%1’을 참조하는 연결이 %2개 있습니다. 먼저 버튼의 "
                       "대상이나 장소의 화면 연결을 변경하세요.")
                        .arg(removed["label"].toString())
                        .arg(found.size()),
                    QMessageBox::Ok, this);
    box.setDetailedText(found.join("\n"));
    box.exec();
    return;
  }
  if (QMessageBox::question(
          this, tr("화면 삭제"),
          tr("‘%1’과 안의 요소를 삭제할까요? 되돌리기로 복구할 수 "
             "있습니다.\n탭만 닫으려면 취소 후 탭의 ×를 누르세요.")
              .arg(removed["label"].toString()),
          QMessageBox::Yes | QMessageBox::Cancel,
          QMessageBox::Cancel) != QMessageBox::Yes)
    return;
  visual["screens"] = kept;
  project["visual"] = visual;
  replaceProject(project);
}
