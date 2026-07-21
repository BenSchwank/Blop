#pragma once
#include "AbstractTool.h"
#include <QGraphicsTextItem>
#include <QTextDocument>
#include <QTextCursor>
#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsScene>
#include <QString>
#include <QTextOption>
#include <QTransform>
#include <QGraphicsItem>
#include "ToolMode.h"

class TextTool : public AbstractTool {

public:
    using AbstractTool::AbstractTool;

    ToolMode mode() const override { return ToolMode::Text; }
    QString name() const override { return "Text"; }
    QString iconName() const override { return "text"; }

    bool handleMousePress(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        if (!scene) return false;
        
        QGraphicsItem* item = scene->itemAt(event->scenePos(), QTransform());
        QGraphicsTextItem* textItem = dynamic_cast<QGraphicsTextItem*>(item);
        
        // If clicking on an existing text item, let it handle the event natively.
        if (textItem) {
            if (m_activeTextItem && m_activeTextItem != textItem) {
                clearEmptyTextItem(scene);
            }
            m_activeTextItem = textItem;
            m_activeTextItem->setFocus();
            return false; 
        }
        
        // If clicking outside, clear focus of any active item
        clearEmptyTextItem(scene);

        // Check if we just want to close the previous text item. If we clicked empty space, 
        // we create a new text item at the cursor.
        QGraphicsTextItem *newText = new QGraphicsTextItem();
        newText->setPos(event->scenePos());
        
        QFont font = newText->font();
        font.setPointSize(m_config.penWidth > 0 ? qBound(8, static_cast<int>(m_config.penWidth), 72) : 16);
        if (!m_config.fontFamily.isEmpty()) {
          if (m_config.fontFamily == QLatin1String("Serif"))
            font.setStyleHint(QFont::Serif);
          else if (m_config.fontFamily == QLatin1String("Mono"))
            font.setStyleHint(QFont::Monospace);
          else if (m_config.fontFamily == QLatin1String("Round"))
            font.setStyleHint(QFont::SansSerif);
          font.setFamily(m_config.fontFamily);
        }
        newText->setFont(font);
        QColor tc = m_config.penColor;
        tc.setAlphaF(qBound(0.15, m_config.opacity, 1.0));
        newText->setDefaultTextColor(tc);

        {
          QTextOption opt = newText->document()->defaultTextOption();
          Qt::Alignment align = Qt::AlignLeft;
          if (m_config.textAlign == 1)
            align = Qt::AlignHCenter;
          else if (m_config.textAlign == 2)
            align = Qt::AlignRight;
          opt.setAlignment(align);
          newText->document()->setDefaultTextOption(opt);
        }
        
        newText->setTextInteractionFlags(Qt::TextEditorInteraction);
        newText->setFlags(QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable | QGraphicsItem::ItemIsMovable);
        
        scene->addItem(newText);
        newText->setFocus();
        
        m_activeTextItem = newText;
        emit contentModified();
        
        return true; // We consumed the event to place the text box
    }

    bool handleMouseMove(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        // Text tool doesn't drag to create.
        return false;
    }

    bool handleMouseRelease(QGraphicsSceneMouseEvent* event, QGraphicsScene* scene) override {
        return false;
    }

private:
   QGraphicsTextItem* m_activeTextItem{nullptr};

   void clearEmptyTextItem(QGraphicsScene* scene) {
       if (m_activeTextItem) {
           m_activeTextItem->clearFocus();
           if (m_activeTextItem->toPlainText().trimmed().isEmpty()) {
               scene->removeItem(m_activeTextItem);
               delete m_activeTextItem;
           }
           m_activeTextItem = nullptr;
       }
   }
};
