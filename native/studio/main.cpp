#include "studio.hpp"
#include "theme.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QTimer>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setOrganizationName("RaisingEngine");
  app.setApplicationName("Raising Studio Qt");
  applyStudioTheme(app);
  const QString bundledRoot =
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../..");
  const QString defaultRoot =
      QFileInfo::exists(bundledRoot + "/apps/qt-studio/service.mjs")
          ? bundledRoot
          : QDir::currentPath();
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOption(
      {"engine-root", "Engine repository directory", "directory", defaultRoot});
  parser.addOption({"project", "Project directory", "directory"});
  parser.addOption({"analysis", "Open archived automatic play results"});
  parser.process(app);
  StudioWindow window(parser.value("engine-root"));
  window.show();
  if (parser.isSet("project"))
    QTimer::singleShot(150, &window,
                       [&] { window.openProject(parser.value("project")); });
  if (parser.isSet("analysis")) {
    auto ready = new QTimer(&window);
    QObject::connect(ready, &QTimer::timeout, &window, [&window, ready] {
      if (window.busy()) return;
      ready->stop();
      if (auto action = window.findChild<QAction *>("showAnalysis")) action->trigger();
    });
    ready->start(500);
  }
  return app.exec();
}
