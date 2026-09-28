#pragma once

#include "ToolMode.h"

#include <QColor>
#include <QObject>
#include <QPainterPath>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QSlider;
class QVariantAnimation;

/// Desktop note toolbar skins (A4 + infinite note). All four drive
/// ToolManager; undo/redo are forwarded to MainWindow via NoteToolbarHost.
enum class NoteToolbarStyle { Bar, Floating, Radiant, Science };

NoteToolbarStyle noteToolbarStyle();
void setNoteToolbarStyle(NoteToolbarStyle style);
QString noteToolbarStyleLabel(NoteToolbarStyle style);

void noteActivateTool(ToolMode mode, int shapeKind = -1);
void noteApplyPenColor(const QColor &color);
void noteApplyPenWidth(int width);

struct NoteToolTile {
  enum Action { Tool, Undo, Redo, More, Color, Wheel, Options, Center };
  QString icon;
  QString label;
  ToolMode mode{ToolMode::Pen};
  int shapeKind{-1};
  Action action{Tool};
  QColor tint;   ///< forced glyph color (highlighter yellow)
  QRectF rect;   ///< hit + paint rect (rect skins)
  QPainterPath path; ///< hit + paint path (radiant)
  QPointF iconCenter;
  qreal iconSize{22};
  qreal radius{10};
};

/// Custom-painted toolbar: shared hover/press/active handling.
class NoteToolbarBase : public QWidget {
  Q_OBJECT
public:
  explicit NoteToolbarBase(QWidget *parent = nullptr);
  void setUndoRedoAvailable(bool canUndo, bool canRedo);
  void refreshActive();

signals:
  void undoRequested();
  void redoRequested();
  void moreRequested(const QPoint &globalPos);
  void toolOptionsRequested();

protected:
  virtual void rebuildTiles() = 0;
  virtual void paintSkin(QPainter &p) = 0;
  virtual void paintTile(QPainter &p, const NoteToolTile &t, int index);
  virtual QColor activeFill() const;
  virtual QColor activeGlyph() const;
  bool tileActive(const NoteToolTile &t) const;
  int tileAt(const QPointF &pos) const;
  void activate(int index);
  void paintGlyphAt(QPainter &p, const QString &icon, const QPointF &center,
                    qreal size, const QColor &color) const;
  void paintTileContent(QPainter &p, const NoteToolTile &t,
                        const QColor &glyphColor) const;
  QColor currentPenColor() const;

  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  QVector<NoteToolTile> m_tiles;
  int m_hover{-1};
  int m_pressed{-1};
  qreal m_pressScale{1.0};
  bool m_canUndo{true};
  bool m_canRedo{false};
  int m_colorStep{0};
  QVariantAnimation *m_pressAnim{nullptr};
};

/// Dark labeled bar (Hand, Stift, Radierer, Auswahl, Formel, Messen, Molekül).
class NoteBarToolbar : public NoteToolbarBase {
  Q_OBJECT
public:
  explicit NoteBarToolbar(QWidget *parent = nullptr);

protected:
  void rebuildTiles() override;
  void paintSkin(QPainter &p) override;
  QColor activeFill() const override;
  QColor activeGlyph() const override;
};

/// Compact dark pill.
class NoteFloatToolbar : public NoteToolbarBase {
  Q_OBJECT
public:
  explicit NoteFloatToolbar(QWidget *parent = nullptr);

protected:
  void rebuildTiles() override;
  void paintSkin(QPainter &p) override;
  QColor activeFill() const override;
  QColor activeGlyph() const override;
};

/// Quarter bloom anchored bottom-right: center disc + two segmented rings.
class NoteRadiantToolbar : public NoteToolbarBase {
  Q_OBJECT
public:
  explicit NoteRadiantToolbar(QWidget *parent = nullptr);

protected:
  void rebuildTiles() override;
  void paintSkin(QPainter &p) override;
  void paintTile(QPainter &p, const NoteToolTile &t, int index) override;

private:
  QPointF m_center;
};

/// Two-column scientific rail with a color wheel at the bottom.
class NoteScienceToolbar : public NoteToolbarBase {
  Q_OBJECT
public:
  explicit NoteScienceToolbar(QWidget *parent = nullptr);

protected:
  void rebuildTiles() override;
  void paintSkin(QPainter &p) override;
  void paintTile(QPainter &p, const NoteToolTile &t, int index) override;
  QColor activeFill() const override;
  QColor activeGlyph() const override;
};

/// Small white chip: pen glyph + width slider (Floating skin only).
class NotePenWidthChip : public QWidget {
  Q_OBJECT
public:
  explicit NotePenWidthChip(QWidget *parent = nullptr);
  void syncFromToolManager();

protected:
  void paintEvent(QPaintEvent *event) override;

private:
  QSlider *m_slider{nullptr};
  bool m_syncing{false};
};

/// Owns the four skins as children of the note surface and positions them.
class NoteToolbarHost : public QObject {
  Q_OBJECT
public:
  explicit NoteToolbarHost(QWidget *surface);

  void setVisible(bool on);
  bool isVisible() const { return m_visible; }
  NoteToolbarStyle style() const;
  void applyStyle();
  /// Positions the active skin inside the surface; `topInset` is the note
  /// header height in surface coordinates.
  void relayout(int topInset);
  void setUndoRedoAvailable(bool canUndo, bool canRedo);
  /// Geometry of the currently shown skin (surface coordinates).
  QRect activeRect() const;
  void raiseAll();

signals:
  void undoRequested();
  void redoRequested();
  void toolOptionsRequested();
  void styleChanged();

private:
  QWidget *activeWidget() const;
  void showStyleMenu(const QPoint &globalPos);

  QPointer<QWidget> m_surface;
  NoteBarToolbar *m_bar{nullptr};
  NoteFloatToolbar *m_float{nullptr};
  NoteRadiantToolbar *m_radiant{nullptr};
  NoteScienceToolbar *m_science{nullptr};
  NotePenWidthChip *m_chip{nullptr};
  bool m_visible{false};
  int m_topInset{0};
};
