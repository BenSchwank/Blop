#pragma once
#include <QString>
#include <QVector>
#include <QPointF>
#include <QPainterPath>
#include <QColor>

struct Stroke {
    QVector<QPointF> points;
    QPainterPath path;
    qreal width{2.0};
    QColor color{Qt::black};
    bool isEraser{false};
    int pageIndex{0};
};

struct NotePage {
    QVector<Stroke> strokes;
};

struct Note {
    QString id;
    QString title;
    QVector<NotePage> pages;
    // Simple helpers
    void ensurePage(int index) {
        if (index >= pages.size()) pages.resize(index + 1);
    }
};
