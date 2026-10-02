#include "wordtextframe.h"

#include <cmath>

#include <QApplication>
#include <QFocusEvent>
#include <QGraphicsSceneEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>
#include <QTextListFormat>
#include <QtMath>

namespace WordText {

namespace {

int gridOf(int grid) { return grid < 8 ? kGrid : grid; }

void applyFamily(QFont &font, const QString &family) {
  if (family.isEmpty())
    return;
  if (family == QLatin1String("Serif"))
    font.setStyleHint(QFont::Serif);
  else if (family == QLatin1String("Mono"))
    font.setStyleHint(QFont::Monospace);
  else if (family == QLatin1String("Round"))
    font.setStyleHint(QFont::SansSerif);
  font.setFamily(family);
}

void mergeChar(QGraphicsTextItem *item, const QTextCharFormat &fmt) {
  if (!item)
    return;
  QTextCursor c = item->textCursor();
  c.mergeCharFormat(fmt);
  if (!c.hasSelection())
    c.setCharFormat(fmt);
  item->setTextCursor(c);
}

bool underStrip(QWidget *w) {
  while (w) {
    if (w->objectName() == QLatin1String("ToolOptionsStrip"))
      return true;
    w = w->parentWidget();
  }
  return false;
}

} // namespace

QPointF snapPoint(QPointF p, int grid) {
  const int g = gridOf(grid);
  p.setX(qRound(p.x() / g) * g);
  p.setY(qRound(p.y() / g) * g);
  return p;
}

qreal frameWidthForPage(qreal pageWidth, int grid) {
  const int g = gridOf(grid);
  const qreal inner = pageWidth - 2.0 * g;
  qreal w = std::floor(inner / qreal(g)) * g;
  if (w < g)
    w = g;
  return w;
}

qreal frameWidthInfinite(int grid) {
  return 16.0 * gridOf(grid);
}

Placement placementFor(const QPointF &local, qreal pageWidth, qreal pageHeight,
                       int grid) {
  const int g = gridOf(grid);
  Placement out;
  const bool infinite = pageWidth <= g * 2.0;
  out.width = infinite ? frameWidthInfinite(g) : frameWidthForPage(pageWidth, g);
  QPointF p = snapPoint(local, g);
  if (!infinite) {
    const qreal maxX = qMax(0.0, pageWidth - g - out.width);
    if (p.x() > maxX)
      p.setX(std::floor(maxX / g) * g);
    if (pageHeight > g && p.y() > pageHeight - g)
      p.setY(std::floor((pageHeight - g) / g) * g);
    if (p.y() < 0)
      p.setY(0);
  }
  out.pos = p;
  return out;
}

bool chromeHoldsFocus() {
  return underStrip(QApplication::focusWidget()) ||
         underStrip(QApplication::activePopupWidget());
}

Item::Item(QGraphicsItem *parent) : QGraphicsTextItem(parent) {}

void Item::setLayoutGrid(int grid) { m_grid = gridOf(grid); }

void Item::setFreeMove(bool on) { m_freeMove = on; }

QVariant Item::itemChange(GraphicsItemChange change, const QVariant &value) {
  if (change == ItemPositionChange && !m_freeMove) {
    return snapPoint(value.toPointF(), m_grid);
  }
  return QGraphicsTextItem::itemChange(change, value);
}

void Item::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
  QMenu menu;
  QAction *freeAct = menu.addAction(QStringLiteral("Frei bewegen"));
  QAction *gridAct = menu.addAction(QStringLiteral("Am Raster"));
  freeAct->setCheckable(true);
  gridAct->setCheckable(true);
  freeAct->setChecked(m_freeMove);
  gridAct->setChecked(!m_freeMove);
  QAction *chosen = menu.exec(event->screenPos());
  if (chosen == freeAct) {
    setFreeMove(true);
  } else if (chosen == gridAct) {
    setFreeMove(false);
    setPos(snapPoint(pos(), m_grid));
  }
  event->accept();
}

void Item::keyPressEvent(QKeyEvent *event) {
  if (handleKey(this, event)) {
    event->accept();
    return;
  }
  QGraphicsTextItem::keyPressEvent(event);
}

void Item::focusOutEvent(QFocusEvent *event) {
  QGraphicsTextItem::focusOutEvent(event);
  if (chromeHoldsFocus())
    return;
  setTextInteractionFlags(Qt::NoTextInteraction);
}

bool Item::event(QEvent *event) {
  if (event->type() == QEvent::ShortcutOverride) {
    auto *key = static_cast<QKeyEvent *>(event);
    if (isFormatKey(key)) {
      event->accept();
      return true;
    }
  }
  return QGraphicsTextItem::event(event);
}

Item *make(QGraphicsItem *parent) {
  auto *item = new Item(parent);
  item->setData(0, QStringLiteral("text_item"));
  item->document()->setDocumentMargin(4);
  QTextOption opt = item->document()->defaultTextOption();
  opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  item->document()->setDefaultTextOption(opt);
  item->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsMovable |
                 QGraphicsItem::ItemIsFocusable |
                 QGraphicsItem::ItemSendsGeometryChanges);
  item->setTextInteractionFlags(Qt::NoTextInteraction);
  item->setZValue(12);
  return item;
}

void loadContent(Item *item, const TextObject &data, int grid) {
  if (!item)
    return;
  item->setLayoutGrid(grid);
  item->setFreeMove(true);
  item->setTextWidth(qMax(qreal(gridOf(grid)), data.width));
  item->setDefaultTextColor(data.color.isValid() ? data.color : Qt::black);
  QFont font = item->font();
  applyFamily(font, data.fontFamily);
  font.setPointSize(qBound(8, data.fontPointSize, 72));
  item->setFont(font);
  if (!data.html.isEmpty())
    item->setHtml(data.html);
  else
    item->setPlainText(data.text);
  item->setPos(data.pos);
  item->setFreeMove(data.freeMove);
}

void applyNewFrameStyle(Item *item, const ToolConfig &cfg) {
  if (!item)
    return;
  const int pt = qBound(10, cfg.penWidth > 0 ? cfg.penWidth : 16, 48);
  QFont font = item->font();
  applyFamily(font, cfg.fontFamily);
  font.setPointSize(pt);
  font.setBold(cfg.textBold);
  font.setItalic(cfg.textItalic);
  font.setUnderline(cfg.textUnderline);
  item->setFont(font);
  const QColor color = cfg.penColor.isValid() ? cfg.penColor : QColor(Qt::black);
  item->setDefaultTextColor(color);

  Qt::Alignment align = Qt::AlignLeft;
  if (cfg.textAlign == 1)
    align = Qt::AlignHCenter;
  else if (cfg.textAlign == 2)
    align = Qt::AlignRight;
  QTextOption opt = item->document()->defaultTextOption();
  opt.setAlignment(align);
  opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  item->document()->setDefaultTextOption(opt);

  QTextCursor c(item->document());
  QTextCharFormat fmt;
  fmt.setFontWeight(cfg.textBold ? QFont::Bold : QFont::Normal);
  fmt.setFontItalic(cfg.textItalic);
  fmt.setFontUnderline(cfg.textUnderline);
  fmt.setFontPointSize(pt);
  if (!cfg.fontFamily.isEmpty())
    fmt.setFontFamilies(QStringList{cfg.fontFamily});
  fmt.setForeground(color);
  c.select(QTextCursor::Document);
  c.mergeCharFormat(fmt);
  c.clearSelection();
  QTextBlockFormat bf = c.blockFormat();
  bf.setAlignment(align);
  c.setBlockFormat(bf);
  if (cfg.textListKind == 1) {
    QTextListFormat lf;
    lf.setStyle(QTextListFormat::ListDisc);
    c.createList(lf);
  } else if (cfg.textListKind == 2) {
    QTextListFormat lf;
    lf.setStyle(QTextListFormat::ListDecimal);
    c.createList(lf);
  }
  item->setTextCursor(c);
}

TextObject toObject(const QGraphicsTextItem *text) {
  TextObject to;
  if (!text)
    return to;
  to.pos = text->pos();
  to.width = text->textWidth();
  to.text = text->toPlainText();
  to.html = text->toHtml();
  to.color = text->defaultTextColor();
  to.fontFamily = text->font().family();
  to.fontPointSize = text->font().pointSize() > 0 ? text->font().pointSize() : 14;
  if (auto *word = dynamic_cast<const Item *>(text))
    to.freeMove = word->freeMove();
  return to;
}

bool isFormatKey(const QKeyEvent *event) {
  if (!event)
    return false;
  const Qt::KeyboardModifiers mods = event->modifiers();
  const bool ctrl = mods.testFlag(Qt::ControlModifier);
  const bool shift = mods.testFlag(Qt::ShiftModifier);
  if (!ctrl && (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab))
    return true;
  if (!ctrl && shift &&
      (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter))
    return true;
  if (!ctrl)
    return false;
  switch (event->key()) {
  case Qt::Key_B:
  case Qt::Key_I:
  case Qt::Key_U:
  case Qt::Key_L:
  case Qt::Key_E:
  case Qt::Key_R:
  case Qt::Key_O:
    return true;
  default:
    return false;
  }
}

bool handleKey(QGraphicsTextItem *item, QKeyEvent *event) {
  if (!item || !event)
    return false;
  const Qt::KeyboardModifiers mods = event->modifiers();
  const bool ctrl = mods.testFlag(Qt::ControlModifier);
  const bool shift = mods.testFlag(Qt::ShiftModifier);

  if (ctrl && shift && event->key() == Qt::Key_L) {
    QTextList *list = item->textCursor().currentList();
    const bool already = list && list->format().style() == QTextListFormat::ListDisc;
    setListKind(item, already ? 0 : 1);
    return true;
  }
  if (ctrl && shift && event->key() == Qt::Key_O) {
    QTextList *list = item->textCursor().currentList();
    const bool already =
        list && list->format().style() == QTextListFormat::ListDecimal;
    setListKind(item, already ? 0 : 2);
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_B) {
    const bool on = item->textCursor().charFormat().fontWeight() <= QFont::Normal;
    setBold(item, on);
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_I) {
    setItalic(item, !item->textCursor().charFormat().fontItalic());
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_U) {
    setUnderline(item, !item->textCursor().charFormat().fontUnderline());
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_L) {
    setAlign(item, Qt::AlignLeft);
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_E) {
    setAlign(item, Qt::AlignHCenter);
    return true;
  }
  if (ctrl && !shift && event->key() == Qt::Key_R) {
    setAlign(item, Qt::AlignRight);
    return true;
  }
  if (!ctrl && shift &&
      (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
    QTextCursor c = item->textCursor();
    c.insertText(QString(QChar::LineSeparator));
    item->setTextCursor(c);
    return true;
  }
  if (!ctrl && (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab)) {
    QTextCursor c = item->textCursor();
    QTextList *list = c.currentList();
    if (!list)
      return false;
    QTextListFormat fmt = list->format();
    const int next = fmt.indent() + ((shift || event->key() == Qt::Key_Backtab) ? -1 : 1);
    fmt.setIndent(qMax(1, next));
    c.createList(fmt);
    item->setTextCursor(c);
    return true;
  }
  return false;
}

void setBold(QGraphicsTextItem *item, bool on) {
  QTextCharFormat fmt;
  fmt.setFontWeight(on ? QFont::Bold : QFont::Normal);
  mergeChar(item, fmt);
}

void setItalic(QGraphicsTextItem *item, bool on) {
  QTextCharFormat fmt;
  fmt.setFontItalic(on);
  mergeChar(item, fmt);
}

void setUnderline(QGraphicsTextItem *item, bool on) {
  QTextCharFormat fmt;
  fmt.setFontUnderline(on);
  mergeChar(item, fmt);
}

void setAlign(QGraphicsTextItem *item, Qt::Alignment align) {
  if (!item)
    return;
  QTextCursor c = item->textCursor();
  QTextBlockFormat bf = c.blockFormat();
  bf.setAlignment(align);
  c.mergeBlockFormat(bf);
  item->setTextCursor(c);
  QTextOption opt = item->document()->defaultTextOption();
  opt.setAlignment(align);
  item->document()->setDefaultTextOption(opt);
}

void setPointSize(QGraphicsTextItem *item, int pt) {
  QTextCharFormat fmt;
  fmt.setFontPointSize(qBound(10, pt, 48));
  mergeChar(item, fmt);
}

void setFamily(QGraphicsTextItem *item, const QString &family) {
  QTextCharFormat fmt;
  fmt.setFontFamilies(QStringList{family});
  mergeChar(item, fmt);
}

void setTextColor(QGraphicsTextItem *item, const QColor &color) {
  QTextCharFormat fmt;
  fmt.setForeground(color);
  mergeChar(item, fmt);
  if (item)
    item->setDefaultTextColor(color);
}

void setListKind(QGraphicsTextItem *item, int kind) {
  if (!item)
    return;
  QTextCursor c = item->textCursor();
  if (QTextList *list = c.currentList())
    list->remove(c.block());
  if (kind == 0)
    return;
  QTextListFormat fmt;
  fmt.setStyle(kind == 2 ? QTextListFormat::ListDecimal
                         : QTextListFormat::ListDisc);
  c.createList(fmt);
  item->setTextCursor(c);
}

} // namespace WordText
