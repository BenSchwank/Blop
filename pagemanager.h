#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSet>
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
    void onSearchChanged(const QString &text);
    void onToggleSelectMode();
    void onSelectAll();
    void onClearSelection();
    void onDeleteSelected();
    void onDuplicateSelected();
    void onMoveSelectedUp();
    void onMoveSelectedDown();
    void onApplyTemplateColor();
    void onGroupFilterChanged(int index);

private:
    void setupUi();
    void fillParent();
    void rebuildList(bool keepSelection = true);
    QList<int> selectedPageIndices() const;
    void setCardSelectedStyle(QListWidgetItem *item, bool selected);
    QString pageGroupKey(int pageIndex) const;
    void updateSubtitle();

    MultiPageNoteView* m_view{nullptr};
    QPushButton* m_scrim{nullptr};
    QWidget* m_panel{nullptr};
    QListWidget* m_listWidget{nullptr};
    QLabel* m_lblTitle{nullptr};
    QLabel* m_lblSubtitle{nullptr};
    QPushButton* m_btnClose{nullptr};
    QPushButton* m_fabAdd{nullptr};
    QLineEdit *m_search{nullptr};
    QPushButton *m_btnSelectMode{nullptr};
    QPushButton *m_btnSelectAll{nullptr};
    QPushButton *m_btnClearSelection{nullptr};
    QPushButton *m_btnDeleteSelection{nullptr};
    QPushButton *m_btnDuplicateSelection{nullptr};
    QPushButton *m_btnMoveUp{nullptr};
    QPushButton *m_btnMoveDown{nullptr};
    QPushButton *m_btnApplyLayout{nullptr};
    QComboBox *m_groupFilter{nullptr};
    bool m_multiSelectMode{false};
    QString m_searchText;
    QSet<int> m_selectedPages;
};
