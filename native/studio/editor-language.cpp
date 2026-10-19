#include "editor-language.hpp"
#include "runtime-paths.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QVBoxLayout>
#include <memory>
#include "editor-diagnostics.inc"

namespace {
QString currentLanguage = "ko";
QString supported(QString value) {
  value = value.toLower().replace('_', '-').section('-', 0, 0);
  return QStringList{"ko", "en", "ja"}.contains(value) ? value : QString();
}
std::unique_ptr<QSettings> preferences() {
  const auto file = qEnvironmentVariable("RAISING_EDITOR_SETTINGS");
  if (!file.isEmpty()) return std::make_unique<QSettings>(file, QSettings::IniFormat);
  return std::make_unique<QSettings>("RaisingEngine", "Raising Studio Qt");
}
class EditorTranslator final : public QTranslator {
  QHash<QString, QString> messages;
public:
  explicit EditorTranslator(const QString &language, QObject *parent) : QTranslator(parent) {
    QFile file(":/editor/translations.json");
    if (!file.open(QIODevice::ReadOnly)) return;
    const int column = language == "ja" ? 2 : 1;
    for (const auto &value : QJsonDocument::fromJson(file.readAll()).array()) {
      const auto row = value.toArray();
      if (row.size() == 3 && !row[column].toString().isEmpty())
        messages.insert(row[0].toString(), row[column].toString());
    }
  }
  bool isEmpty() const override { return messages.isEmpty(); }
  QString translate(const char *, const char *source, const char * = nullptr, int = -1) const override {
    // Exact source matches only. Never rewrite user data, IDs, or service errors.
    return messages.value(QString::fromUtf8(source));
  }
};
}

QString editorLanguage() { return currentLanguage; }
QString savedEditorLanguage() {
  auto saved = supported(preferences()->value("editor/language").toString());
  if (!saved.isEmpty()) return saved;
  const auto system = supported(QLocale::system().name());
  return system.isEmpty() ? "en" : system;
}
bool saveEditorLanguage(const QString &language) {
  if (supported(language) != language) return false;
  auto settings = preferences();
  settings->setValue("editor/language", language);
  settings->sync();
  return settings->status() == QSettings::NoError;
}
bool installEditorLanguage(QApplication &app, const QString &requested) {
  Q_INIT_RESOURCE(editor_translations);
  if (app.property("editorLanguageInstalled").toBool()) return false;
  const auto language = requested.isEmpty() ? savedEditorLanguage() : supported(requested);
  if (language.isEmpty()) return false;
  auto qt = new QTranslator(&app);
  const auto translations = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
  if (qt->load("qtbase_" + language, QCoreApplication::applicationDirPath() + "/translations") ||
      qt->load("qtbase_" + language, translations))
    app.installTranslator(qt);
  else delete qt;
  if (language != "ko") {
    auto catalog = new EditorTranslator(language, &app);
    if (catalog->isEmpty()) { delete catalog; return false; }
    app.installTranslator(catalog);
  }
  currentLanguage = language;
  app.setProperty("editorLanguageInstalled", true);
  auto font = app.font();
  const auto fontId=QFontDatabase::addApplicationFont(studioEngineRoot()+"/dist/native-player-app/fonts/NotoSansKR.ttf");
  const auto bundledFonts=QFontDatabase::applicationFontFamilies(fontId);
  auto jpPath=QCoreApplication::applicationDirPath()+"/fonts/NotoSansJP.ttf";
  if(!QFile::exists(jpPath))jpPath=studioEngineRoot()+"/native/studio/fonts/NotoSansJP.ttf";
  const auto japaneseFonts=QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(jpPath));
  QStringList families=language == "ja" ? QStringList{"Yu Gothic UI", "Meiryo", "Noto Sans CJK JP"}
                                       : QStringList{"Malgun Gothic", "Noto Sans KR", "Segoe UI"};
  const auto installed=QFontDatabase::families();
  for(int i=families.size()-1;i>=0;--i)if(!installed.contains(families[i]))families.removeAt(i);
  if(language=="ja")families=japaneseFonts+families;
  families.append(bundledFonts);families.append(japaneseFonts);families.append("sans-serif");
  font.setFamilies(families);
  app.setFont(font);
  return true;
}
QWidget *editorLanguagePicker(QWidget *parent) {
  auto widget = new QWidget(parent);
  auto layout = new QVBoxLayout(widget); layout->setContentsMargins(0, 0, 0, 0);
  auto row = new QHBoxLayout;
  row->addWidget(new QLabel("편집기 언어 / Editor language / エディター言語"));
  auto combo = new QComboBox;
  combo->setObjectName("editorLanguage");
  combo->addItem("한국어", "ko"); combo->addItem("English", "en"); combo->addItem("日本語", "ja");
  combo->setCurrentIndex(combo->findData(savedEditorLanguage())); row->addWidget(combo); row->addStretch();
  layout->addLayout(row);
  auto hint = new QLabel(QObject::tr("변경한 편집기 언어는 다음 실행부터 적용됩니다. 게임 언어는 다국어 작업실에서 따로 설정합니다."));
  hint->setObjectName("editorLanguageHint"); hint->setWordWrap(true); layout->addWidget(hint);
  QObject::connect(combo, &QComboBox::activated, widget, [combo, hint] {
    if (saveEditorLanguage(combo->currentData().toString()))
      hint->setText("저장됨 · 다시 실행하면 적용 / Saved · restart to apply / 保存済み・再起動後に適用");
    else hint->setText("설정 저장 실패 / Could not save preference / 設定を保存できませんでした");
  });
  return widget;
}
void showEditorLanguageSettings(QWidget *parent) {
  QDialog dialog(parent); dialog.setObjectName("editorLanguageDialog");
  dialog.setWindowTitle("언어 / Language / 言語");
  auto layout = new QVBoxLayout(&dialog); layout->addWidget(editorLanguagePicker(&dialog));
  auto close = new QDialogButtonBox(QDialogButtonBox::Close); layout->addWidget(close);
  QObject::connect(close, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  dialog.exec();
}
