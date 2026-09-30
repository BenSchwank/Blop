#pragma once

#include <QColor>
#include <QHash>
#include <QPaintEvent>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QToolButton;
class QVBoxLayout;

/// K-style charcoal icon rail (far left of the library shell).
class LibraryIconRail : public QWidget {
  Q_OBJECT
public:
  explicit LibraryIconRail(QWidget *parent = nullptr);

  int preferredWidth() const;
  void setActiveId(const QString &id);
  void setAvatarLetter(const QString &letter);
  void setAccentColor(const QColor &color);
  /// Blop logo, or the picture chosen in Einstellungen → Logo.
  void setBrandPixmap(const QPixmap &pm);

  /// ToolButton for a given rail id, or nullptr if unknown.
  QToolButton *buttonFor(const QString &id) const;

signals:
  void actionTriggered(const QString &id);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QToolButton *addBtn(const QString &id, const QString &iconKey,
                      const QString &tip, QVBoxLayout *lay);
  void refreshStyles();
  void applyBrand();

  QHash<QString, QToolButton *> m_btns;
  QLabel *m_logo{nullptr};
  QPixmap m_brand;
  QString m_active{QStringLiteral("home")};
  QColor m_accent;
  QString m_avatar;
};
