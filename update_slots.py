import sys

with open('multipagenoteview.h', 'r', encoding='utf-8') as f:
    text = f.read()

new_slots = """
public slots:
    void onSelectionChanged();
    void deleteSelection();
    void copySelection();
    void cutSelection();
    void duplicateSelection();
    void changeSelectionColor();
    void startTransformSession();
    void applyTransform();
    void screenshotSelection();
"""

text = text.replace("public slots:\n    void onSelectionChanged();", new_slots)
text = text.replace("public slots:\r\n    void onSelectionChanged();", new_slots)

with open('multipagenoteview.h', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(text)

with open('multipagenoteview.cpp', 'r', encoding='utf-8') as f:
    cpp_text = f.read()

implementations = """
void MultiPageNoteView::deleteSelection() {
    for (auto item : scene_.selectedItems()) {
        scene_.removeItem(item);
        delete item;
    }
    if (m_selectionMenu) m_selectionMenu->hide();
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::copySelection() {
    const QList<QGraphicsItem*> selected = scene_.selectedItems();
    if (selected.isEmpty()) return;
    QRectF selRect;
    for (auto *i : selected) selRect = selRect.united(i->sceneBoundingRect());
    if (!selRect.isEmpty()) {
        QImage img(selRect.size().toSize(), QImage::Format_ARGB32);
        img.fill(Qt::transparent);
        QPainter p(&img);
        scene_.render(&p, QRectF(0, 0, img.width(), img.height()), selRect);
        QGuiApplication::clipboard()->setImage(img);
    }
    if (m_selectionMenu) m_selectionMenu->hide();
}

void MultiPageNoteView::cutSelection() {
    copySelection();
    deleteSelection();
}

void MultiPageNoteView::duplicateSelection() {}

void MultiPageNoteView::changeSelectionColor() {
    QColor c = QColorDialog::getColor(Qt::black, this, "Farbe ändern");
    if (c.isValid()) {
        for (auto *item : scene_.selectedItems()) {
            if (auto *pi = dynamic_cast<QGraphicsPathItem *>(item)) {
                QPen p = pi->pen();
                p.setColor(c);
                pi->setPen(p);
            }
        }
        scene_.update();
    }
    if (m_selectionMenu) m_selectionMenu->hide();
    if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::startTransformSession() {
  QList<QGraphicsItem *> items = scene_.selectedItems();
  if (items.isEmpty()) return;
  if (items.count() == 1) {
    QGraphicsItem *singleItem = items.first();
    QPointF oldCenter = singleItem->sceneBoundingRect().center();
    singleItem->setTransformOriginPoint(singleItem->boundingRect().center());
    QPointF newCenter = singleItem->sceneBoundingRect().center();
    singleItem->moveBy(oldCenter.x() - newCenter.x(), oldCenter.y() - newCenter.y());
  }
  if (m_selectionMenu) m_selectionMenu->hide();
}

void MultiPageNoteView::applyTransform() {
  if (onSaveRequested) onSaveRequested(note_);
}

void MultiPageNoteView::screenshotSelection() {
    copySelection(); 
}
"""

if "void MultiPageNoteView::copySelection()" not in cpp_text:
    cpp_text += "\n" + implementations

old_connects1 = """  connect(&scene_, &QGraphicsScene::selectionChanged, this, &MultiPageNoteView::onSelectionChanged);
  connect(m_selectionMenu, &NoteSelectionMenu::deleteRequested, this, [this]() {
      for (auto item : scene_.selectedItems()) {
          scene_.removeItem(item);
          delete item;
      }
      m_selectionMenu->hide();
  });"""

old_connects2 = old_connects1.replace('\n', '\r\n')

new_connects = """  connect(&scene_, &QGraphicsScene::selectionChanged, this, &MultiPageNoteView::onSelectionChanged);
  connect(m_selectionMenu, &NoteSelectionMenu::deleteRequested, this, &MultiPageNoteView::deleteSelection);
  connect(m_selectionMenu, &NoteSelectionMenu::copyRequested, this, &MultiPageNoteView::copySelection);
  connect(m_selectionMenu, &NoteSelectionMenu::cutRequested, this, &MultiPageNoteView::cutSelection);
  connect(m_selectionMenu, &NoteSelectionMenu::transformRequested, this, &MultiPageNoteView::startTransformSession);
  connect(m_selectionMenu, &NoteSelectionMenu::colorRequested, this, &MultiPageNoteView::changeSelectionColor);
  connect(m_selectionMenu, &NoteSelectionMenu::screenshotRequested, this, &MultiPageNoteView::screenshotSelection);
"""

cpp_text = cpp_text.replace(old_connects1, new_connects)
cpp_text = cpp_text.replace(old_connects2, new_connects)

with open('multipagenoteview.cpp', 'w', encoding='utf-8', newline='\r\n') as f:
    f.write(cpp_text)
print("Done!")
