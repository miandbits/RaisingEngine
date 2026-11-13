#pragma once
#include <QFileInfo>
#include <QDir>
#include <QStringList>

struct ExternalToolLaunch {QString program;QStringList arguments;};
enum class ExternalToolPlatform {Windows,MacOS,Unix};
inline ExternalToolPlatform externalToolPlatform(){
#if defined(Q_OS_WIN)
  return ExternalToolPlatform::Windows;
#elif defined(Q_OS_MACOS)
  return ExternalToolPlatform::MacOS;
#else
  return ExternalToolPlatform::Unix;
#endif
}
// Never interpret a command line: the project path remains one literal argument.
inline ExternalToolLaunch externalToolLaunch(QString path,QString project,ExternalToolPlatform platform=externalToolPlatform()){
  const QFileInfo file(path);
  if(platform==ExternalToolPlatform::MacOS&&file.isDir()&&file.suffix().compare("app",Qt::CaseInsensitive)==0){
    const QDir bundle(file.absoluteFilePath());
    if(!QFileInfo(bundle.filePath("Contents/Info.plist")).isFile()||!QFileInfo(bundle.filePath("Contents/MacOS")).isDir())return {};
    return {"/usr/bin/open",{"-a",file.absoluteFilePath(),QFileInfo(project).absoluteFilePath()}};
  }
  if(!file.isFile())return {};
  if(platform==ExternalToolPlatform::Windows){if(file.suffix().compare("exe",Qt::CaseInsensitive)!=0)return {};}
  else if(!file.isExecutable())return {};
  return {file.absoluteFilePath(),{QFileInfo(project).absoluteFilePath()}};
}
