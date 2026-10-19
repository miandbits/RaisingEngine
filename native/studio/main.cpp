#include "studio.hpp"
#include "project-manager.hpp"
#include "theme.hpp"
#include "runtime-paths.hpp"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QTimer>

#include "editor-language.hpp"
int main(int argc, char **argv) {
  QApplication app(argc, argv);
  app.setOrganizationName("RaisingEngine");
  app.setApplicationName("Raising Studio Qt");
  const auto bundledPlayer=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("player-runtime");
  if(qEnvironmentVariableIsEmpty("RAISING_NATIVE_PLAYER_RUNTIME")&&QFileInfo::exists(bundledPlayer+"/build.json"))
    qputenv("RAISING_NATIVE_PLAYER_RUNTIME",bundledPlayer.toUtf8());
  applyStudioTheme(app);
  const QString defaultRoot = studioEngineRoot();
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addOption(
      {"engine-root", "Engine repository directory", "directory", defaultRoot});
  parser.addOption({"project", "Project directory", "directory"});
  parser.addOption({"ui-language", "Editor language: ko, en, ja (does not change game language)", "language"});
  parser.addOption({"analysis", "Open archived automatic play results"});
  parser.addOption({"workspace", "Open integrated authoring workspace"});
  parser.process(app);
  if (!installEditorLanguage(app, parser.value("ui-language"))) return 2;
  QString project = parser.value("project");
  if (project.isEmpty()) {
    ProjectManager manager(parser.value("engine-root"));
    if (manager.exec() != QDialog::Accepted) return 0;
    project = manager.selectedProject();
  }
  StudioWindow window(parser.value("engine-root"));
  window.show();
  if (!project.isEmpty())
    QTimer::singleShot(150, &window,
                       [&] { window.openProject(project); });
  if (parser.isSet("analysis")) {
    auto ready = new QTimer(&window);
    QObject::connect(ready, &QTimer::timeout, &window, [&window, ready] {
      if (window.busy()) return;
      ready->stop();
      if (auto action = window.findChild<QAction *>("showAnalysis")) action->trigger();
    });
    ready->start(500);
  }
  if (parser.isSet("workspace")) {
    auto ready = new QTimer(&window);
    QObject::connect(ready, &QTimer::timeout, &window, [&window, ready] {
      if (window.busy()) return;
      ready->stop();
      if (auto action = window.findChild<QAction *>("showAuthoringWorkspace")) action->trigger();
    });
    ready->start(500);
  }
  return app.exec();
}
