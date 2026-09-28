#pragma once

#include "dashboardlayoutstore.h"

#include <QFrame>
#include <QString>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;
class QHBoxLayout;

/// Soft card chrome for a dashboard board widget.
class DashWidget : public QFrame {
  Q_OBJECT
public:
  explicit DashWidget(const QString &id, QWidget *parent = nullptr);

  QString blockId() const { return m_id; }
  void setEditMode(bool on);
  void setPhoneMode(bool phone);
  void setBody(QWidget *body);
  void refreshContent();
  void setLifted(bool lifted);
  void setSizeClass(DashSizeClass sizeClass);
  DashSizeClass sizeClass() const { return m_sizeClass; }
  void applyChromePrefs(bool bgEnabled, bool borderEnabled,
                        const QString &bgColor, const QString &borderColor);

  static DashWidget *create(const QString &id, QWidget *parent = nullptr);

signals:
  void openNotePath(const QString &path);
  void newNoteRequested();
  void snapToNotesRequested();
  void studyRequested();
  void searchLibrary(const QString &text);
  void maximizeCalendarRequested();
  void contentChanged();
  void dragHandlePressed(const QPoint &globalPos);
  void resizeHandlePressed(const QPoint &globalPos);
  void sizeClassPicked(DashSizeClass sizeClass);
  void removeBannerRequested(const QString &id);
  void chromePrefsChanged(bool bgEnabled, bool borderEnabled,
                          const QString &bgColor, const QString &borderColor);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void rebuildBody();
  QWidget *buildToday();
  QWidget *buildTodos();
  QWidget *buildCalendar();
  QWidget *buildRecent();
  QWidget *buildShortcuts();
  QWidget *buildBanner();
  QWidget *buildIntro();
  void applyChrome();
  void layoutResizeHandle();
  void updateEditAffordances();
  void adaptHeaderDensity();
  void showStyleMenu();
  bool isCompact() const;
  int maxListItems() const;
  QString elideForWidth(const QString &text, int widthPx, int fontPx) const;
  static int cardRadiusDp();

  QString m_id;
  bool m_editMode{false};
  bool m_phone{false};
  bool m_lifted{false};
  bool m_bgEnabled{true};
  bool m_borderEnabled{true};
  QString m_bgColor;
  QString m_borderColor;
  DashSizeClass m_sizeClass{DashSizeClass::M};
  QVBoxLayout *m_root{nullptr};
  QWidget *m_header{nullptr};
  QLabel *m_title{nullptr};
  QLabel *m_grip{nullptr};
  QPushButton *m_btnStyle{nullptr};
  QWidget *m_resizeHandle{nullptr};
  QWidget *m_bodyHost{nullptr};
  QVBoxLayout *m_bodyLay{nullptr};
};
