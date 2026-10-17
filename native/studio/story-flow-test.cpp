#include "story-flow.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <iostream>

int main(int argc,char**argv){
  QCoreApplication app(argc,argv);
  // Test-only bridge lets runtime tests consume exactly the editor's compiler output.
  if(argc==3){QFile input(QString::fromLocal8Bit(argv[1]));if(!input.open(QIODevice::ReadOnly))return 2;
    const auto data=QJsonDocument::fromJson(input.readAll()).object();const auto plan=makeStoryFlowPlan(data.value("view").toObject(),data.value("settings").toObject());
    QFile output(QString::fromLocal8Bit(argv[2]));if(!output.open(QIODevice::WriteOnly))return 3;
    output.write(QJsonDocument(QJsonObject{{"error",plan.error},{"edits",plan.edits},{"settings",plan.settings}}).toJson());return 0;
  }
  std::cerr<<"usage: qt-story-flow-test input.json output.json\n";return 2;
}
