#pragma once

#include <QColor>
#include <QEvent>
#include <QHash>
#include <QPaintEvent>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QToolButton;
class QVBoxLayout;
class QHideEvent;

/// K-style charcoal icon rail (far left of the library shell).
class LibraryIconRail : public QWidget {
  Q_OBJECT
public:
  explicit LibraryIconRail(QWidget *parent = nullptr);

  int preferredWidth() const;
  void setActiveId(const QString &id);
  void setAvatarLetter(const QString &letter);
  void setAccentColor(const QColor &color);
  /// Right hairline, drawn only while a note frames the rail as the L's leg.
  void setInnerEdge(bool on);
  /// Blop logo, or the picture chosen in Einstellungen → Logo.
  void setBrandPixmap(const QPixmap &pm);

  /// ToolButton for a given rail id, or nullptr if unknown.
  QToolButton *buttonFor(const QString &id) const;

signals:
  void actionTriggered(const QString &id);
  /// Logo is the main-menu button. The sidebar slide is the transition.
  void logoActivated();

protected:
  void paintEvent(QPaintEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QToolButton *addBtn(const QString &id, const QString &iconKey,
                      const QString &tip, QVBoxLayout *lay);
  void refreshStyles();
  void applyBrand();
  void showFlyout(QToolButton *btn);
  void hideFlyout();

  QHash<QString, QToolButton *> m_btns;
  QLabel *m_logo{nullptr};
  QLabel *m_flyout{nullptr};
  QPixmap m_brand;
  QString m_active{QStringLiteral("home")};
  QColor m_accent;
  bool m_innerEdge{false};
  QString m_avatar;
};
