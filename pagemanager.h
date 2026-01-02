#pragma once
#include <QWidget>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QLabel>
#include "multipagenoteview.h"

// --- Delegate für die Darstellung der Seiten-Thumbnails ---
class PageListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PageListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

// --- PageManager Overlay Widget ---
class PageManager : public QWidget {
    Q_OBJECT
public:
    explicit PageManager(QWidget* parent = nullptr);

    void setNoteView(MultiPageNoteView* view);
    void refreshThumbnails();

signals:
    void pageSelected(int index);
    void pageOrderChanged();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    MultiPageNoteView* m_view{nullptr};
    QListWidget* m_listWidget;
    QPushButton* m_btnClose;
    QLabel* m_lblTitle;

    void setupUi();
    void onRowMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
};
