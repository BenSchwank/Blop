#pragma once
#include <QWidget>
#include <QListWidget>
#include <QStyledItemDelegate>
#include <QPushButton>
#include <QLabel>
#include "multipagenoteview.h"

// --- Delegate: Zeichnet Thumbnail + 3-Punkte-Menü ---
class PageListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PageListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    // Fängt Klicks auf das Menü ab
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void menuRequested(const QPoint& globalPos, int index);
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
    void pageOrderChanged(); // Signalisiert, dass sich die Struktur geändert hat

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onRowMoved(const QModelIndex& parent, int start, int end, const QModelIndex& destination, int row);
    void showContextMenu(const QPoint& pos, int index);

private:
    MultiPageNoteView* m_view{nullptr};
    QListWidget* m_listWidget;
    QPushButton* m_btnClose;
    QLabel* m_lblTitle;

    // Zwischenspeicher für Kopieren (optional für später)
    NotePage m_copiedPage;
    bool m_hasCopiedPage{false};

    void setupUi();
};
