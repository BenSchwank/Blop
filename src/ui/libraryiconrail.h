#pragma once

#include <QColor>
#include <QHash>
#include <QPaintEvent>
#include <QWidget>

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

signals:
  void actionTriggered(const QString &id);

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QToolButton *addBtn(const QString &id, const QString &iconKey,
                      const QString &tip, QVBoxLayout *lay);
  void refreshStyles();

  QHash<QString, QToolButton *> m_btns;
  QString m_active{QStringLiteral("library")};
  QColor m_accent{QColor(QStringLiteral("#5B9DFF"))};
  QString m_avatar{QStringLiteral("B")};
};
