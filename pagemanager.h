#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QStyledItemDelegate>
#include "multipagenoteview.h"

class QKeyEvent;
class QShowEvent;

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
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void showContextMenu(const QPoint& pos, int index);
    void onRowMoved(const QModelIndex&, int start, int, const QModelIndex&, int row);
    void onAddPage();
    void onListItemClicked(QListWidgetItem* item);

private:
    void setupUi();
    void fillParent();

    MultiPageNoteView* m_view{nullptr};
    QPushButton* m_scrim{nullptr};
    QWidget* m_panel{nullptr};
    QListWidget* m_listWidget{nullptr};
    QLabel* m_lblTitle{nullptr};
    QLabel* m_lblSubtitle{nullptr};
    QPushButton* m_btnClose{nullptr};
    QPushButton* m_fabAdd{nullptr};
};
