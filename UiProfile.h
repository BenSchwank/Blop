#pragma once
#include <QString>
#include <QJsonObject>

struct UiProfile {
    QString id;
    QString name;

    // Einstellbare Parameter
    int folderIconSize{24}; // NEU: Nur für Ordner/Dateien
    int uiIconSize{24};     // NEU: Für Toolbar/Buttons
    int buttonSize{40};     // Größe des Buttons an sich (Rahmen)

    int gridItemSize{100};   // Breite
    int gridItemHeight{80};  // Höhe
    int gridSpacing{20};

    bool isTouchOptimized{false};

    static UiProfile fromJson(const QJsonObject& obj) {
        UiProfile p;
        p.id = obj["id"].toString();
        p.name = obj["name"].toString();

        // Fallback für alte Profile: iconSize wird zu beiden
        int oldIcon = obj["iconSize"].toInt(24);
        p.folderIconSize = obj["folderIconSize"].toInt(oldIcon);
        p.uiIconSize = obj["uiIconSize"].toInt(oldIcon);

        p.buttonSize = obj["buttonSize"].toInt(40);
        p.gridItemSize = obj["gridItemSize"].toInt(100);
        p.gridItemHeight = obj["gridItemHeight"].toInt(80);
        p.gridSpacing = obj["gridSpacing"].toInt(20);
        p.isTouchOptimized = obj["isTouchOptimized"].toBool(false);
        return p;
    }

    QJsonObject toJson() const {
        return {
            {"id", id},
            {"name", name},
            {"folderIconSize", folderIconSize},
            {"uiIconSize", uiIconSize},
            {"buttonSize", buttonSize},
            {"gridItemSize", gridItemSize},
            {"gridItemHeight", gridItemHeight},
            {"gridSpacing", gridSpacing},
            {"isTouchOptimized", isTouchOptimized}
        };
    }

    static UiProfile createDesktop() {
        UiProfile p; p.id = "desktop"; p.name = "Desktop Standard";
        p.folderIconSize = 48; p.uiIconSize = 20;
        p.buttonSize = 40;
        p.gridItemSize = 120; p.gridItemHeight = 80;
        p.gridSpacing = 20; p.isTouchOptimized = false;
        return p;
    }

    static UiProfile createTablet() {
        UiProfile p; p.id = "tablet"; p.name = "Tablet Groß";
        p.folderIconSize = 64; p.uiIconSize = 32;
        p.buttonSize = 60;
        p.gridItemSize = 180; p.gridItemHeight = 110;
        p.gridSpacing = 30; p.isTouchOptimized = true;
        return p;
    }
};
