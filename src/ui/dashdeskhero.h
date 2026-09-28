#pragma once

#include <QPixmap>
#include <QPoint>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPushButton;
class QWheelEvent;

/// Fill body for a dashboard banner — wash preset, cover image, or crop.
class DashDeskHero : public QWidget {
  Q_OBJECT
public:
  explicit DashDeskHero(const QString &bannerId, QWidget *parent = nullptr);

  void setEditMode(bool on);
  void reloadCover();

signals:
  void removeRequested();

protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  enum class Wash { Soft, Mist, Dusk };

  void layoutEditBtns();
  void showLibraryMenu();
  void showSecondaryMenu();
  void pickCover();
  void applyWash(Wash wash);
  void resetCover();
  void resetCrop();
  void loadPrefs();
  void saveCropPrefs() const;
  void clearCoverFile();
  QString coverSettingsKey() const;
  QString washSettingsKey() const;
  QString focusXKey() const;
  QString focusYKey() const;
  QString zoomKey() const;
  QString coverStorageDir() const;
  void loadCoverPixmap();
  void styleEditBtns();
  void paintWash(QPainter &p, Wash wash, const QColor &acc, bool dark);
  void paintCover(QPainter &p, const QColor &acc, bool dark);
  Wash washFromId(const QString &id) const;
  QString washId(Wash w) const;

  QString m_bannerId;
  bool m_editMode{false};
  QPixmap m_cover;
  QPixmap m_logo;
  Wash m_wash{Wash::Soft};
  qreal m_focusX{0.5};
  qreal m_focusY{0.5};
  qreal m_zoom{1.0};
  bool m_panning{false};
  QPoint m_panLast;
  QPushButton *m_btnPick{nullptr};
  QPushButton *m_btnMore{nullptr};
};
