#pragma once
#include <QWidget>
#include <QTimer>
#include <QJsonObject>
#include <functional>
class ScreenCanvas;
class RendererPreviewDock;
class QCheckBox;
class QComboBox;
class QLabel;
class QMainWindow;
class CanvasStateBar : public QWidget {
  Q_OBJECT
public:
  CanvasStateBar(ScreenCanvas* canvas,QMainWindow* window);
  void updateContext(QJsonObject view,QString screen,QString locale,QSize viewport);
  RendererPreviewDock* renderer() const {return preview;}
  std::function<bool()> busy;
signals:
  void snapshotRequested();
private:
  ScreenCanvas* canvas;
  RendererPreviewDock* preview;
  QCheckBox* enabled;
  QComboBox* actor;
  QLabel* status;
  QTimer debounce;
  QString root,key,attempted;
  bool filling=false;
  void changed();
  void refresh();
  void remember();
  void applySnapshot();
};
