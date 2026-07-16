#pragma once

// DocumentTabBar - floating squircle tabs for open notes, Drawboard-style.
//
// Home stays pinned; open-note tabs live in a horizontal scroll strip so many
// documents never crush the chrome. The active tab keeps a sliding accent
// underline (same motion tokens as ModernToolbar).

#include <QColor>
#include <QList>
#include <QWidget>

class QHBoxLayout;
class QPropertyAnimation;
class QScrollArea;
class DocumentTab;

class DocumentTabBar : public QWidget {
  Q_OBJECT
public:
  explicit DocumentTabBar(QWidget *parent = nullptr);

  int addTab(const QString &title, const QString &iconName = QStringLiteral("note_bnote"));
  void removeTab(int index);
  void setTabText(int index, const QString &title);

  int currentIndex() const;
  int count() const;

  void setAccentColor(const QColor &color);
  void setHomeActive(bool active);
  /// Hide the built-in home squircle when the host already has a Home button.
  void setHomeVisible(bool visible);

signals:
  void currentChanged(int index);
  void tabCloseRequested(int index);
  void homeClicked();

public slots:
  void setCurrentIndex(int index);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  void updateIndicator(bool animate = true);
  void ensureTabVisible(int index);
  void rebindTabSignals();
  DocumentTab *tabAt(int index) const;

  QHBoxLayout *m_outerLayout{nullptr};
  QScrollArea *m_scroll{nullptr};
  QWidget *m_scrollContent{nullptr};
  QHBoxLayout *m_tabsLayout{nullptr};
  QList<DocumentTab *> m_tabs;
  int m_currentIndex{-1};
  QColor m_accentColor{QColor(QStringLiteral("#7C5CFC"))};

  QWidget *m_indicator{nullptr};
  QPropertyAnimation *m_indicatorAnim{nullptr};

  DocumentTab *m_homeTab{nullptr};
  bool m_homeActive{true};
};

// -----------------------------------------------------------------------------
// DocumentTab - one floating squircle tab button.
// -----------------------------------------------------------------------------
class DocumentTab : public QWidget {
  Q_OBJECT
public:
  explicit DocumentTab(const QString &title, const QString &iconName,
                       bool closable, QWidget *parent = nullptr);

  void setActive(bool active, const QColor &accent);
  void setAccentColor(const QColor &color);
  void setTitle(const QString &title);
  bool isActive() const { return m_active; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;
  QSize iconTextSize() const;

signals:
  void clicked();
  void closeClicked();

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  void refreshTitleLabel();

  QString m_title;
  QString m_iconName;
  QColor m_accentColor{QColor(QStringLiteral("#7C5CFC"))};
  bool m_active{false};
  bool m_hovered{false};
  bool m_closable{true};
  class QLabel *m_textLbl{nullptr};
};
