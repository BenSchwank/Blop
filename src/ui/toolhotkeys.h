#pragma once

#include "ToolMode.h"

#include <QColor>
#include <QKeySequence>
#include <QString>
#include <QVector>

struct ToolHotkeyDef {
  QString id;
  ToolMode mode;
  QString label;
  QString fallbackKeys;
  bool penPreset = false;
  QColor fallbackColor = Qt::black;
  int fallbackWidth = 3;
};

const QVector<ToolHotkeyDef> &toolHotkeyDefs();

struct ResolvedHotkey {
  QString id;
  ToolMode mode;
  QString label;
  QKeySequence sequence;
  bool penPreset = false;
  QColor color = Qt::black;
  int width = 3;
};

QVector<ResolvedHotkey> resolvedToolHotkeys();
QString openRouterModel();
QString openRouterKey();
void storeToolHotkey(const QString &id, const QString &portableKeys,
                     const QColor &color, int width);
void storeOpenRouter(const QString &model, const QString &key);
void resetToolHotkeys();
QString assistantSettingsPath();
bool publishAssistantSettings(QString *error = nullptr);
