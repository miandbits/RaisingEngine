#pragma once
#include "bridge.hpp"
#include <QDialog>

class QLineEdit;
class QListWidget;
class QLabel;
class QPushButton;
class QComboBox;

// User-local launch history is separate from game data and engine preferences.
class ProjectManager final : public QDialog {
  Q_OBJECT
public:
  explicit ProjectManager(const QString &engineRoot, QWidget *parent = nullptr);
  QString selectedProject() const { return selected; }
  static void remember(const QString &root);
  static QStringList recentProjects();
  void reject() override;
private:
  void refresh();
  void open(const QString &path);
  void submit(const QString &method, QJsonObject params);
  void setBusy(bool busy);
  StudioBridge bridge;
  QString selected;
  QListWidget *recent;
  QLineEdit *search, *location, *destination;
  QComboBox *base;
  QLabel *message;
  QPushButton *openButton, *removeButton;
  QWidget *controls;
};
