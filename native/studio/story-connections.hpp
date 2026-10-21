#pragma once
#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <functional>

struct StoryConnectionNode {
  QString id, kind, title, detail, file, scriptNode, choice;
};
struct StoryConnectionEdge {
  QString from, to, reason;
  bool conditional = false;
};
struct StoryConnections {
  QMap<QString, StoryConnectionNode> nodes;
  QList<StoryConnectionEdge> edges;
};
// Static authoring dependencies, never a proof of runtime reachability.
StoryConnections buildStoryConnections(const QJsonArray &documents);
QSet<QString> storyConnectionNeighborhood(const StoryConnections &, const QString &focus, int depth);

class StoryConnectionsDialog final : public QDialog {
public:
  StoryConnectionsDialog(QJsonObject context, QString file, QWidget *parent,
    std::function<QJsonObject(QString,QString,QString,QWidget*)> editConditions,
    std::function<QJsonObject(QJsonArray)> applyDraft = {});
  QString selectedFile, selectedNode;
};
