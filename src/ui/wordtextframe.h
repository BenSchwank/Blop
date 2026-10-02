#pragma once

#include "Note.h"
#include "ToolSettings.h"

#include <QGraphicsTextItem>
#include <QPointF>

class QKeyEvent;
class QWidget;

namespace WordText {

constexpr int kGrid = 40;

struct Placement {
  QPointF pos;
  qreal width{640};
};

QPointF snapPoint(QPointF p, int grid);
qreal frameWidthForPage(qreal pageWidth, int grid);
qreal frameWidthInfinite(int grid);
Placement placementFor(const QPointF &local, qreal pageWidth, qreal pageHeight,
                       int grid);

bool chromeHoldsFocus();

class Item : public QGraphicsTextItem {
public:
  explicit Item(QGraphicsItem *parent = nullptr);

  void setLayoutGrid(int grid);
  int layoutGrid() const { return m_grid; }
  void setFreeMove(bool on);
  bool freeMove() const { return m_freeMove; }

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
  void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  bool event(QEvent *event) override;

private:
  int m_grid{kGrid};
  bool m_freeMove{false};
};

Item *make(QGraphicsItem *parent = nullptr);
void loadContent(Item *item, const TextObject &data, int grid);
void applyNewFrameStyle(Item *item, const ToolConfig &cfg);
TextObject toObject(const QGraphicsTextItem *text);

bool isFormatKey(const QKeyEvent *event);
bool handleKey(QGraphicsTextItem *item, QKeyEvent *event);

void setBold(QGraphicsTextItem *item, bool on);
void setItalic(QGraphicsTextItem *item, bool on);
void setUnderline(QGraphicsTextItem *item, bool on);
void setAlign(QGraphicsTextItem *item, Qt::Alignment align);
void setPointSize(QGraphicsTextItem *item, int pt);
void setFamily(QGraphicsTextItem *item, const QString &family);
void setTextColor(QGraphicsTextItem *item, const QColor &color);
void setListKind(QGraphicsTextItem *item, int kind);

} // namespace WordText
