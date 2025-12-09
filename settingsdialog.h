#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QColor>
#include <QWidget>
#include <QPainter>
#include "UiProfile.h"

namespace Ui { class SettingsDialog; }

class QListWidget;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QListWidgetItem;

// --- Vorschau-Widget Klasse ---
class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumWidth(250);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    int folderIcon{24};
    int uiIcon{24};
    int btnSize{40};
    int gridItemW{100};
    int gridItemH{80};
    int gridSpacing{20};
    bool touch{false};

    void updateValues(int fIcon, int uIcon, int btn, int w, int h, int space, bool t) {
        folderIcon = fIcon;
        uiIcon = uIcon;
        btnSize = btn;
        gridItemW = w;
        gridItemH = h;
        gridSpacing = space;
        touch = t;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        p.fillRect(rect(), QColor(0x161925));
        p.setPen(QPen(QColor(0x444), 1));
        p.drawRect(rect().adjusted(0,0,-1,-1));

        int startX = 20;
        int currentY = 20;

        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        p.setFont(f);

        // 1. GRID VORSCHAU (Nutzt folderIcon)
        p.drawText(startX, currentY + 10, "Dateien (Raster):");
        currentY += 25;

        for(int i=0; i<2; ++i) {
            int x = startX + i * (gridItemW + gridSpacing);
            if (x + gridItemW > width()) break;

            QRect itemRect(x, currentY, gridItemW, gridItemH);

            p.setBrush(QColor(255, 255, 255, 10));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(itemRect, 8, 8);

            // Ordner Icon
            int icS = folderIcon;
            if (icS > gridItemW - 10) icS = gridItemW - 10; // Clip protection
            QRect iconRect(0, 0, icS, icS);
            // Zentriert, leicht nach oben verschoben
            iconRect.moveCenter(QPoint(itemRect.center().x(), itemRect.top() + itemRect.height()/2 - 8));

            p.setBrush(QColor(0x5E5CE6));
            p.drawEllipse(iconRect);

            // Text Dummy
            p.setPen(QColor(200,200,200));
            QRect textRect = itemRect;
            textRect.setTop(iconRect.bottom() + 4);
            f.setBold(false);
            f.setPointSize(touch ? 10 : 8);
            p.setFont(f);
            p.drawText(textRect, Qt::AlignCenter, "Datei " + QString::number(i+1));
        }

        currentY += gridItemH + 40;

        // 2. BUTTON VORSCHAU (Nutzt uiIcon)
        f.setBold(true); f.setPointSize(10); p.setFont(f); p.setPen(Qt::white);
        p.drawText(startX, currentY, "Toolbar Buttons:");
        currentY += 15;

        QRect btnRect(startX, currentY, btnSize, btnSize);
        p.setBrush(QColor(0x333));
        p.setPen(QPen(QColor(0x555), 1));
        p.drawRoundedRect(btnRect, 5, 5);

        p.setBrush(Qt::white); p.setPen(Qt::NoPen);

        // UI Icon
        int uS = uiIcon;
        if(uS > btnSize - 4) uS = btnSize - 4;
        p.drawEllipse(btnRect.center(), uS/2, uS/2);

        p.setPen(Qt::gray);
        p.drawText(btnRect.right() + 15, btnRect.center().y() + 5, QString("Btn: %1px | Icon: %2px").arg(btnSize).arg(uiIcon));
    }
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setToolbarConfig(bool isRadial, bool isHalf, int scalePercent);

    // Dummies
    void setTouchMode(bool) {}
    void setGridValues(int, int) {}

signals:
    void accentColorChanged(QColor color);
    void toolbarStyleChanged(bool radial);
    void toolbarScaleChanged(int percent);

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::SettingsDialog *ui;

    QListWidget* m_profileList;
    QWidget* m_editorOverlay;

    // Editor Felder
    QLineEdit* edName;
    QSpinBox* edFolderIcon; // Umbenannt
    QSpinBox* edUiIcon;     // NEU
    QSpinBox* edBtn;
    QSpinBox* edGridItem;
    QSpinBox* edGridItemH;  // NEU
    QSpinBox* edGridSpace;
    QCheckBox* edTouch;

    PreviewWidget* m_preview;
    QString m_editingId;

    void setupDesignTab();
    void refreshProfileList();
    void showProfileEditor(const UiProfile& p);
    void saveProfileFromEditor();

    void updatePreview();

    QWidget* createProfileListItem(const UiProfile& p);
};

#endif // SETTINGSDIALOG_H
