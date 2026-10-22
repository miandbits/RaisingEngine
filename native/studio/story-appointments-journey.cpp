// Test driver: operates the same dialogs and emits the normal atomic edit batch.
// The service applies/saves it with revision checks; no game JSON is authored here.
#include "script-authoring.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <iostream>

int main(int argc,char **argv){
  QApplication app(argc,argv);
  // Offscreen Windows does not enumerate system font fallback like the windowing plugin.
  const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/malgun.ttf");
  if(font>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).first(),10));
  if(argc!=4){std::cerr<<"Usage: context.json edit-batch.json screenshots-directory\n";return 2;}
  auto check=[](bool ok,const QString &message){if(!ok){std::cerr<<message.toStdString()<<std::endl;std::exit(1);}};
  QFile input(argv[1]);check(input.open(QIODevice::ReadOnly),"Cannot open context");const auto request=QJsonDocument::fromJson(input.readAll()).object();
  const auto context=request.value("view").toObject();const auto cases=request.value("cases").toArray();QDir().mkpath(argv[3]);
  ScriptAuthoringDialog author(context,{});author.show();app.processEvents();
  int number=0;
  for(auto value:cases){const auto item=value.toObject();const int shot=++number;author.selectDocument(item.value("sourceFile").toString());
    QTimer::singleShot(0,&author,[&author,&app,check,item,shot,argv]{
      auto map=author.findChild<QDialog*>("storyConnectionsDialog");check(map,"Connection map missing");auto focus=map->findChild<QComboBox*>("connectionsFocus");const int index=focus->findData(item.value("source").toString());check(index>=0,"Source choice not in map");focus->setCurrentIndex(index);
      QTimer::singleShot(0,map,[map,&app,check,item,shot,argv]{
        auto form=map->findChild<QDialog*>("storyFollowupDialog");check(form,"Follow-up dialog missing");
        auto select=[&](const char *name,const QJsonValue &value){auto combo=form->findChild<QComboBox*>(name);const int at=combo->findData(value.toVariant());check(at>=0,QString("Missing option: ")+name+" / "+value.toVariant().toString());combo->setCurrentIndex(at);};
        if(item.contains("targetFile"))select("followupTarget",item["targetFile"]);
        else{form->findChild<QLineEdit*>("followupTitle")->setText(item.value("title").toString());form->findChild<QPlainTextEdit*>("followupDialogue")->setPlainText(item.value("dialogue").toString());}
        select("followupTrigger",item["trigger"]);select("followupFrom",item["fromPeriod"]);select("followupThrough",item["throughPeriod"]);
        if(item.contains("activityId"))select("followupActivity",item["activityId"]);if(item.contains("locationId"))select("followupLocation",item["locationId"]);
        auto promise=form->findChild<QComboBox*>("followupPromise");check(promise->count()>1,"No personal promise offered");promise->setCurrentIndex(1);
        app.processEvents();check(form->grab().save(QString::fromLocal8Bit(argv[3])+QString("/form-%1.png").arg(shot)),"Form screenshot failed");
        form->findChild<QDialogButtonBox*>("followupButtons")->button(QDialogButtonBox::Save)->click();check(form->result()==QDialog::Accepted,form->findChild<QLabel*>("followupError")->text());
      });
      map->findChild<QPushButton*>("connectionsFollowup")->click();app.processEvents();map->grab().save(QString::fromLocal8Bit(argv[3])+QString("/map-%1.png").arg(shot));map->reject();
    });
    author.findChild<QPushButton*>("scriptConnections")->click();
  }
  bool saved=false;author.setApplyHandler([&](QJsonArray edits,std::function<void(QString)> done){QFile output(argv[2]);check(output.open(QIODevice::WriteOnly),"Cannot write edit batch");output.write(QJsonDocument(edits).toJson());saved=true;done({});});
  author.findChild<QDialogButtonBox*>("scriptButtons")->button(QDialogButtonBox::Save)->click();check(saved&&author.result()==QDialog::Accepted,author.findChild<QLabel*>("scriptError")->text());
  std::cout<<"PASS "<<cases.size()<<" follow-ups authored through editor map/dialog/save widgets\n";
}
