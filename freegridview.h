#ifndef FREEGRIDVIEW_H
#define FREEGRIDVIEW_H

#include <QListView>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

class FreeGridView : public QListView
{
    Q_OBJECT

public:
    explicit FreeGridView(QWidget *parent = nullptr);

    void setAccentColor(const QColor &c) { m_accentColor = c; }

    // Setzt die visuelle Größe des Items (Klickbereich)
    void setItemSize(const QSize &size);
    QSize itemSize() const { return m_itemSize; }

protected:
    void paintEvent(QPaintEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    QRect m_ghostRect;
    bool m_showGhost;
    QColor m_accentColor;
    QSize m_itemSize; // Die Größe des Inhalts (Ordner)

    QPoint calculateSnapPosition(const QPoint &pos);
};

#endif // FREEGRIDVIEW_H
