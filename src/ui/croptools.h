#pragma once

// v3.18.0: gemeinsame Crop-Bausteine für CanvasView und MultiPageNoteView.
// Vorher lebten CropMenu + CropResizer als lokale Klassen in canvasview.cpp;
// die Extraktion erlaubt es, die Crop-Session auch im .bnote-Editor
// (MultiPageNoteView) anzubieten, ohne ~200 Zeilen zu duplizieren.

#include <QGraphicsObject>
#include <QRectF>
#include <QWidget>

class QPushButton;

/// Schwebende Leiste mit Rechteck/Freihand-Umschalter + Apply/Cancel.
/// `showModeButtons=false` blendet den Modus-Umschalter aus (nur Rechteck-
/// Crop, z.B. im MultiPageNoteView, wo Freihand-Lasso nicht verfügbar ist).
class CropMenu : public QWidget {
  Q_OBJECT
public:
  explicit CropMenu(QWidget *parent = nullptr, bool showModeButtons = true);
  void setMode(bool rect);

signals:
  void rectRequested();
  void freehandRequested();
  void applyRequested();
  void cancelRequested();

private:
  QPushButton *btnRect{nullptr};
  QPushButton *btnFree{nullptr};
};

/// Rechteck mit 8 Drag-Handles für den Rechteck-Crop-Modus.
class CropResizer : public QGraphicsObject {
public:
  enum Handle {
    None,
    TopLeft,
    Top,
    TopRight,
    Left,
    Right,
    BottomLeft,
    Bottom,
    BottomRight,
    Center
  };

  explicit CropResizer(QRectF rect);

  QRectF boundingRect() const override;
  void paint(QPainter *painter, const QStyleOptionGraphicsItem *,
             QWidget *) override;
  QRectF currentRect() const;

protected:
  void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

private:
  Handle handleAt(QPointF pos);

  QRectF m_rect;
  QRectF m_startRect;
  QPointF m_startPos;
  Handle m_dragHandle{None};
};
