#pragma once
#include "AbstractTool.h"
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QFont>

/// Sticky note: yellow card with editable text (persisted via NotePage::stickies).
class StickyNoteTool : public AbstractTool {
public:
  using AbstractTool::AbstractTool;

  ToolMode mode() const override { return ToolMode::StickyNote; }
  QString name() const override { return QStringLiteral("Notiz"); }
  QString iconName() const override { return QStringLiteral("stickynote"); }

  bool handleMousePress(QGraphicsSceneMouseEvent *event,
                        QGraphicsScene *scene) override {
    if (!scene)
      return false;

    const QPointF pos = event->scenePos();
    const qreal w = 168.0;
    const qreal h = 148.0;

    auto *card = new QGraphicsRectItem(0, 0, w, h);
    card->setPos(pos);
    QColor fill = m_config.stickyBgColor.isValid() ? m_config.stickyBgColor
                  : (m_config.penColor.isValid() ? m_config.penColor
                                                 : QColor(255, 236, 120));
    if (fill.lightness() < 100)
      fill = QColor(255, 236, 120);
    fill.setAlphaF(qBound(0.35, m_config.opacity, 1.0));
    card->setBrush(fill);
    card->setPen(QPen(QColor(210, 175, 55), 1.2));
    card->setFlags(QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemSendsGeometryChanges);
    card->setZValue(6);
    card->setData(0, QStringLiteral("sticky_note"));

    auto *text = new QGraphicsTextItem(card);
    text->setPos(10, 10);
    text->setTextWidth(w - 20);
    QFont font = text->font();
    const int pt =
        m_config.penWidth > 0 ? qBound(11, m_config.penWidth, 28) : 14;
    font.setPointSize(pt);
    if (!m_config.fontFamily.isEmpty())
      font.setFamily(m_config.fontFamily);
    text->setFont(font);
    text->setDefaultTextColor(QColor(40, 36, 20));
    text->setPlainText(QString());
    text->setTextInteractionFlags(Qt::TextEditorInteraction);
    text->setFlag(QGraphicsItem::ItemIsFocusable, true);

    scene->addItem(card);
    text->setFocus();
    m_lastCompletedItem = card;
    emit contentModified();
    return true;
  }
};
