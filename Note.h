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
    bool isHighlighter{false};
    int pageIndex{0};
};

struct NotePage {
    QString title; // Speichert den Namen der Seite
    QVector<Stroke> strokes;
};

struct Note {
    QString id;
    QString title;
    QVector<NotePage> pages;

    void ensurePage(int index) {
        if (index >= pages.size()) {
            pages.resize(index + 1);
            // Automatische Benennung: "Seite 1", "Seite 2" usw.
            pages[index].title = QString("Seite %1").arg(index + 1);
        }
    }
};
