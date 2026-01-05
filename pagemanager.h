#pragma once

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QStyledItemDelegate>
#include "multipagenoteview.h"

// Delegate für das Karten-Design
class PageListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PageListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void menuRequested(const QPoint& globalPos, int index);
};

class PageManager : public QDialog {
    Q_OBJECT
public:
    explicit PageManager(QWidget* parent = nullptr);

    void setNoteView(MultiPageNoteView* view);
    void refreshThumbnails();

signals:
    void pageSelected(int index);
    void pageOrderChanged();

protected:
    // Hier passiert die Positionierung (Rechtsbündig)
    void showEvent(QShowEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void showContextMenu(const QPoint& pos, int index);
    void onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row);
    void onAddPage();

private:
    void setupUi();
    void alignToRight(); // Hilfsfunktion

    MultiPageNoteView* m_view{nullptr};
    QListWidget* m_listWidget;
    QLabel* m_lblTitle;
    QPushButton* m_btnClose;
    QPushButton* m_fabAdd;
};
