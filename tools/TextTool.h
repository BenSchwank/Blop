#pragma once
#include "AbstractTool.h"
#include "wordtextframe.h"
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QTransform>
#include "ToolMode.h"

class TextTool : public AbstractTool {

public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Text; }
    QString name() const override { return "Text"; }
    QString iconName() const override { return "text"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        m_placedNew = false;

        QGraphicsItem* item = scene->itemAt(event->scenePos(), QTransform());
        while (item && item->data(0).toString() != QLatin1String("text_item"))
            item = item->parentItem();
        auto* textItem = dynamic_cast<QGraphicsTextItem*>(item);

        if (textItem) {
            if (m_activeTextItem && m_activeTextItem != textItem)
                clearEmptyTextItem(scene);
            m_activeTextItem = textItem;
            return false;
        }

        clearEmptyTextItem(scene);

        int grid = scene->property("wordGrid").toInt();
        if (grid < 8)
            grid = WordText::kGrid;
        const qreal pageW = scene->property("wordPageWidth").toDouble();
        const qreal pageH = scene->property("wordPageHeight").toDouble();
        const WordText::Placement place =
            WordText::placementFor(event->scenePos(), pageW, pageH, grid);

        auto* word = WordText::make();
        TextObject data;
        data.pos = place.pos;
        data.width = place.width;
        data.color = m_config.penColor.isValid() ? m_config.penColor : QColor(Qt::black);
        data.fontFamily = m_config.fontFamily;
        data.fontPointSize = qBound(10, m_config.penWidth > 0 ? m_config.penWidth : 16, 48);
        WordText::loadContent(word, data, grid);
        WordText::applyNewFrameStyle(word, m_config);
        word->setTextInteractionFlags(Qt::TextEditorInteraction);
        scene->addItem(word);
        word->setFocus(Qt::MouseFocusReason);
        m_activeTextItem = word;
        m_lastCompletedItem = word;
        m_placedNew = true;
        emit contentModified();
        return true;
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent*, QGraphicsScene*) override {
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent*, QGraphicsScene*) override {
        const bool placed = m_placedNew;
        m_placedNew = false;
        return placed;
    }

private:
   QGraphicsTextItem* m_activeTextItem{nullptr};
   bool m_placedNew{false};

   void clearEmptyTextItem(QGraphicsScene* scene) {
       if (m_activeTextItem) {
           m_activeTextItem->clearFocus();
           if (m_activeTextItem->toPlainText().trimmed().isEmpty()) {
               scene->removeItem(m_activeTextItem);
           }
           m_activeTextItem = nullptr;
       }
   }
};
