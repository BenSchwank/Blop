#pragma once
#include <QString>
#include <QUuid>
#include <QJsonObject>

struct UiProfile {
    QString id;
    QString name;

    // Konfigurierbare Parameter
    int iconSize{100};          // Größe der Datei-Icons (px)
    int gridSpacing{20};        // Abstand im Raster
    double toolbarScale{1.0};   // Skalierung der ModernToolbar (0.5 - 2.0)
    int buttonSize{40};         // Größe von UI-Buttons (px)
    bool snapToGrid{true};      // Rasterung aktivieren

    UiProfile() : id(QUuid::createUuid().toString()), name("Neues Profil") {}

    // Hilfsmethoden für JSON Serialisierung
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["iconSize"] = iconSize;
        obj["gridSpacing"] = gridSpacing;
        obj["toolbarScale"] = toolbarScale;
        obj["buttonSize"] = buttonSize;
        obj["snapToGrid"] = snapToGrid;
        return obj;
    }

    static UiProfile fromJson(const QJsonObject &obj) {
        UiProfile p;
        if (obj.contains("id")) p.id = obj.value("id").toString();
        if (obj.contains("name")) p.name = obj.value("name").toString();
        if (obj.contains("iconSize")) p.iconSize = obj.value("iconSize").toInt(100);
        if (obj.contains("gridSpacing")) p.gridSpacing = obj.value("gridSpacing").toInt(20);
        if (obj.contains("toolbarScale")) p.toolbarScale = obj.value("toolbarScale").toDouble(1.0);
        if (obj.contains("buttonSize")) p.buttonSize = obj.value("buttonSize").toInt(40);
        if (obj.contains("snapToGrid")) p.snapToGrid = obj.value("snapToGrid").toBool(true);
        return p;
    }

    // Hilfsfunktion um zu entscheiden, ob wir im "Touch Modus" sind (für Delegates)
    bool isTouchOptimized() const {
        return buttonSize >= 50;
    }
};
