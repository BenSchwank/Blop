#include "toolhotkeys.h"

#include "cloudlink.h"
#include "storageprefs.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>

namespace {

QString blobKey() { return QStringLiteral("ui/tool_hotkeys"); }

QJsonObject defaultBlob() {
  QJsonObject bindings;
  auto put = [&](const ToolHotkeyDef &def) {
    QJsonObject row;
    row.insert(QStringLiteral("keys"), def.fallbackKeys);
    if (def.penPreset) {
      row.insert(QStringLiteral("color"), def.fallbackColor.name(QColor::HexRgb));
      row.insert(QStringLiteral("width"), def.fallbackWidth);
    }
    bindings.insert(def.id, row);
  };
  for (const ToolHotkeyDef &def : toolHotkeyDefs())
    put(def);
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("openrouterModel"),
              QStringLiteral("openai/gpt-4o-mini"));
  root.insert(QStringLiteral("openrouterKey"), QString());
  root.insert(QStringLiteral("bindings"), bindings);
  return root;
}

qint64 blobStamp(const QJsonObject &obj) {
  return obj.value(QStringLiteral("updatedAt")).toVariant().toLongLong();
}

QJsonObject loadBlob() {
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  const QByteArray raw = s.value(blobKey()).toByteArray();
  QJsonObject obj = QJsonDocument::fromJson(raw).object();
  QFile file(assistantSettingsPath());
  if (file.open(QIODevice::ReadOnly)) {
    const QJsonObject fromFile = QJsonDocument::fromJson(file.readAll()).object();
    if (!fromFile.isEmpty() &&
        (obj.isEmpty() || blobStamp(fromFile) > blobStamp(obj)))
      obj = fromFile;
  }
  if (obj.isEmpty())
    return defaultBlob();
  QJsonObject blob = defaultBlob();
  blob.insert(QStringLiteral("openrouterModel"),
              obj.value(QStringLiteral("openrouterModel"))
                  .toString(blob.value(QStringLiteral("openrouterModel")).toString()));
  blob.insert(QStringLiteral("openrouterKey"),
              obj.value(QStringLiteral("openrouterKey")).toString());
  if (obj.contains(QStringLiteral("voiceHotkey")))
    blob.insert(QStringLiteral("voiceHotkey"),
                obj.value(QStringLiteral("voiceHotkey")).toString());
  QJsonObject bindings = blob.value(QStringLiteral("bindings")).toObject();
  const QJsonObject saved = obj.value(QStringLiteral("bindings")).toObject();
  for (auto it = saved.begin(); it != saved.end(); ++it) {
    if (bindings.contains(it.key()) && it.value().isObject())
      bindings.insert(it.key(), it.value().toObject());
  }
  blob.insert(QStringLiteral("bindings"), bindings);
  if (blobStamp(obj) > 0)
    blob.insert(QStringLiteral("updatedAt"), blobStamp(obj));
  return blob;
}

void saveBlob(const QJsonObject &blob) {
  QJsonObject stamped = blob;
  stamped.insert(QStringLiteral("updatedAt"),
                 QDateTime::currentMSecsSinceEpoch());
  QSettings s(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  s.setValue(blobKey(), QJsonDocument(stamped).toJson(QJsonDocument::Compact));
  s.sync();
}

const ToolHotkeyDef *findDef(const QString &id) {
  for (const ToolHotkeyDef &def : toolHotkeyDefs()) {
    if (def.id == id)
      return &def;
  }
  return nullptr;
}

} // namespace

const QVector<ToolHotkeyDef> &toolHotkeyDefs() {
  static const QVector<ToolHotkeyDef> defs = {
      {QStringLiteral("pen1"), ToolMode::Pen, QStringLiteral("Stift 1"),
       QStringLiteral("Ctrl+1"), true, QColor(Qt::black), 3},
      {QStringLiteral("pen2"), ToolMode::Pen, QStringLiteral("Stift 2"),
       QStringLiteral("Ctrl+2"), true, QColor(QStringLiteral("#2F6FED")), 3},
      {QStringLiteral("pen3"), ToolMode::Pen, QStringLiteral("Stift 3"),
       QStringLiteral("Ctrl+3"), true, QColor(QStringLiteral("#E23B3B")), 3},
      {QStringLiteral("marker"), ToolMode::Highlighter, QStringLiteral("Textmarker"),
       QStringLiteral("Ctrl+4"), false, QColor(), 3},
      {QStringLiteral("pen"), ToolMode::Pen, QStringLiteral("Stift"),
       QStringLiteral("P"), false, QColor(), 3},
      {QStringLiteral("eraser"), ToolMode::Eraser, QStringLiteral("Radierer"),
       QStringLiteral("E"), false, QColor(), 3},
      {QStringLiteral("lasso"), ToolMode::Lasso, QStringLiteral("Lasso"),
       QStringLiteral("V"), false, QColor(), 3},
      {QStringLiteral("text"), ToolMode::Text, QStringLiteral("Text"),
       QStringLiteral("T"), false, QColor(), 3},
      {QStringLiteral("hand"), ToolMode::Hand, QStringLiteral("Hand"),
       QStringLiteral("H"), false, QColor(), 3},
      {QStringLiteral("markerKey"), ToolMode::Highlighter, QStringLiteral("Marker"),
       QStringLiteral("M"), false, QColor(), 3},
  };
  return defs;
}

QVector<ResolvedHotkey> resolvedToolHotkeys() {
  const QJsonObject bindings =
      loadBlob().value(QStringLiteral("bindings")).toObject();
  QVector<ResolvedHotkey> out;
  for (const ToolHotkeyDef &def : toolHotkeyDefs()) {
    ResolvedHotkey hk;
    hk.id = def.id;
    hk.mode = def.mode;
    hk.label = def.label;
    hk.penPreset = def.penPreset;
    hk.color = def.fallbackColor;
    hk.width = def.fallbackWidth;
    const QJsonObject row = bindings.value(def.id).toObject();
    const QString keys = row.value(QStringLiteral("keys")).toString(def.fallbackKeys);
    hk.sequence = QKeySequence(keys, QKeySequence::PortableText);
    if (def.penPreset) {
      const QColor c(row.value(QStringLiteral("color")).toString());
      if (c.isValid())
        hk.color = c;
      const int w = row.value(QStringLiteral("width")).toInt(def.fallbackWidth);
      hk.width = qBound(1, w, 40);
    }
    out.append(hk);
  }
  return out;
}

QString openRouterModel() {
  const QString model =
      loadBlob().value(QStringLiteral("openrouterModel")).toString().trimmed();
  return model.isEmpty() ? QStringLiteral("openai/gpt-4o-mini") : model;
}

QString openRouterKey() {
  return loadBlob().value(QStringLiteral("openrouterKey")).toString().trimmed();
}

void storeToolHotkey(const QString &id, const QString &portableKeys,
                     const QColor &color, int width) {
  if (!findDef(id))
    return;
  QJsonObject blob = loadBlob();
  QJsonObject bindings = blob.value(QStringLiteral("bindings")).toObject();
  QJsonObject row = bindings.value(id).toObject();
  row.insert(QStringLiteral("keys"), portableKeys);
  if (const ToolHotkeyDef *def = findDef(id); def && def->penPreset) {
    row.insert(QStringLiteral("color"),
               color.isValid() ? color.name(QColor::HexRgb)
                               : def->fallbackColor.name(QColor::HexRgb));
    row.insert(QStringLiteral("width"), qBound(1, width, 40));
  }
  bindings.insert(id, row);
  blob.insert(QStringLiteral("bindings"), bindings);
  saveBlob(blob);
}

void storeOpenRouter(const QString &model, const QString &key) {
  QJsonObject blob = loadBlob();
  blob.insert(QStringLiteral("openrouterModel"), model.trimmed());
  blob.insert(QStringLiteral("openrouterKey"), key.trimmed());
  saveBlob(blob);
}

void resetToolHotkeys() {
  QJsonObject blob = loadBlob();
  const QString model = blob.value(QStringLiteral("openrouterModel")).toString();
  const QString key = blob.value(QStringLiteral("openrouterKey")).toString();
  blob = defaultBlob();
  blob.insert(QStringLiteral("openrouterModel"), model);
  blob.insert(QStringLiteral("openrouterKey"), key);
  saveBlob(blob);
}

QString assistantSettingsPath() {
  return StoragePrefs::ensureLocalLibraryRoot() +
         QStringLiteral("/assistent-einstellungen.json");
}

bool publishAssistantSettings(QString *error) {
  const QString path = assistantSettingsPath();
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error)
      *error = QStringLiteral("Die Einstellungsdatei ließ sich nicht schreiben.");
    return false;
  }
  file.write(QJsonDocument(loadBlob()).toJson(QJsonDocument::Indented));
  if (!file.commit()) {
    if (error)
      *error = QStringLiteral("Die Einstellungsdatei ließ sich nicht schreiben.");
    return false;
  }
  if (CloudLinkHub::instance().providerReady(QStringLiteral("googledrive"))) {
    if (!CloudLinkHub::instance().putNote(path) && error)
      *error = CloudLinkHub::instance().lastError();
  }
  return true;
}
