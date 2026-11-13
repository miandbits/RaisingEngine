#pragma once
#include <QJsonObject>
#include <QString>
#include <functional>
class QWidget;
class QDialog;
namespace StudioReleaseCenter {
using Done=std::function<void(QJsonObject)>;
using Request=std::function<void(QString,QJsonObject,Done)>;
QDialog* create(QWidget* parent,const QString& engineRoot,const QString& projectRoot,Request request,std::function<void()> exportGame);
void offerExport(QDialog* dialog,const QJsonObject& exported);
QDialog* showIdentity(QWidget* parent,Request request);
}
