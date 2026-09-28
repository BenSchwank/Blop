#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class QPushButton;

namespace SettingsUi {

bool useSettingsPaper();
QColor settingsContentBg();
QColor settingsContentRowBg();
void paintSettingsContentSurface(QWidget *w,
                                 const QString &objectName = QString());
void applyStoredQss(QWidget *w);
void setThemedQss(QWidget *w, const QString &raw);
void setTokenQss(QWidget *w, const char *kind);
void setLiteralQss(QWidget *w, const QString &raw);
void setSurfaceQss(QWidget *w, const QString &name);
void refreshThemedTree(QWidget *root);

QString accentRgba(int alpha);
QString settingsInk();
QString settingsInkMuted();
QString settingsChipBg();
QString segmentedControlQss();
int settingsSegmentMinHeight();
void styleSegmentTrack(QWidget *track);
void retintSettingsControls(QWidget *root);
QString propertyRowShellQss(bool last);
QString propertyActionQss(bool destructive = false);

void tagSearchKeys(QWidget *w, const QString &keys);
bool widgetMatchesSearch(QWidget *w, const QString &needle);
bool sectionMatchesSearch(QWidget *sectionRoot, const QString &needle,
                          const QString &title, const QString &subtitle,
                          const QString &sectionKeywords = QString());
void applyRowSearchFilter(QWidget *sectionRoot, const QString &needle);

QWidget *makePropertyRow(QWidget *parent, const QString &label, QWidget *action,
                         bool last = false, const QString &searchKeys = QString());
QWidget *makeNamedPropertyRow(QWidget *parent, const QString &title,
                              const QString &status, QWidget *actions,
                              bool last = false,
                              const QString &searchKeys = QString());
QPushButton *makeQuietAction(QWidget *parent, const QString &text,
                             bool destructive = false);

} // namespace SettingsUi
