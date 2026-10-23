#pragma once
#include <QDockWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <QTimer>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>

class RenderImage;
class RendererPreviewDock : public QDockWidget {
  Q_OBJECT
public:
  explicit RendererPreviewDock(QWidget* parent=nullptr);
  ~RendererPreviewDock() override;
  void setContext(const QJsonObject& project,QString screen,QString selection,int revision,QString root,QString locale);
  void launch(QJsonObject descriptor);
  void fail(QString message);
  bool running() const {return pending;}
  QString reportPath() const {return reportFile;}
  QJsonObject result() const {return renderedUi;}
  void begin();
  void setDocuments(QJsonArray value){documents=value;}
  QJsonObject previewRequest() const {return requestedScenario;}
  void setCanvasMode(bool value){canvasMode=value;}
  void setCanvasViewport(QSize size);
  void configureScenario(){editScenario();}
  QJsonObject scenarioSettings() const {return scenario;}
  void setScenarioSettings(QJsonObject value){scenario=value;changed();}
  QString currentKey() const {return contextKey();}
  bool snapshotCurrent() const {return !pending&&renderedContext==contextKey();}
  QImage snapshotImage() const;
  QString message() const {return status->text();}
  QJsonObject explanation() const {return renderedExplanation;}
signals:
  void contextChanged();
  void snapshotRequested();
  void snapshotReady(bool ok);
  void elementPicked(QString id);
private:
  QProcess process;
  QTimer timeout;
  QComboBox *resolution,*locale,*interaction,*element;
  QSpinBox *width,*height;
  QPushButton *render,*cancel,*pin;
  QLabel *status,*caption,*referenceCaption;
  QTreeWidget *nodes;
  QTreeWidget *reasons;
  QJsonArray documents;
  QJsonObject scenario,requestedScenario;
  void editScenario();
  void showReasons(const QJsonObject& explanation);
  RenderImage *image,*reference;
  QJsonObject project,renderedUi;
  QJsonObject renderedExplanation;
  QString screen,root,reportFile,imageFile,renderedContext,requestContext;
  QString requestedScreen,requestedLocale,requestedElement,requestedState;
  QString requestedRoot,renderedRoot,renderedScreen;
  QString contextSelection;
  int revision=0,requestedRevision=0,requestedWidth=0,requestedHeight=0;
  bool pending=false,filling=false;
  bool canvasMode=false;
  QString contextKey() const;
  void changed();
  void complete(int code);
};
