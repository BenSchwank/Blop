#include "settingsdialog.h"
#include "ui_SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QSlider>
#include <QColorDialog>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    // KORREKTUR: Wir löschen KEINE Buttons via setupColorButton mehr, da sie eh entfernt werden.

    // SCROLL AREA LOGIK FÜR TAB DESIGN
    QWidget* tabDesign = ui->tabWidget->findChild<QWidget*>("tabDesign");
    if (!tabDesign) tabDesign = ui->tabWidget->widget(0);

    // KORREKTUR: ALLES löschen, was im Tab ist.
    // qDeleteAll(children()) löscht alle Kinder (Widgets, Layouts) rekursiv.
    // Das verhindert die "Geister"-Buttons und das doppelte UI.
    if (tabDesign) {
        qDeleteAll(tabDesign->children());
        // Zur Sicherheit auch das Layout entfernen, falls es separat existiert
        if (tabDesign->layout()) delete tabDesign->layout();
    }

    // Neues Layout aufbauen
    QVBoxLayout* mainTabLay = new QVBoxLayout(tabDesign);
    mainTabLay->setContentsMargins(0,0,0,0);

    QScrollArea* scroll = new QScrollArea(tabDesign);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");

    QWidget* contentWidget = new QWidget();
    contentWidget->setStyleSheet("background: transparent;");
    QVBoxLayout* contentLay = new QVBoxLayout(contentWidget);
    contentLay->setSpacing(20);
    contentLay->setContentsMargins(20,20,20,20);

    scroll->setWidget(contentWidget);
    mainTabLay->addWidget(scroll);

    // --- INHALT AUFBAUEN ---

    // 1. UI Modus
    QGroupBox* grpUI = new QGroupBox("Benutzeroberfläche", contentWidget);
    grpUI->setStyleSheet("QGroupBox { border: 1px solid #333; border-radius: 8px; font-weight: bold; color: #888; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout* layUI = new QVBoxLayout(grpUI);
    QRadioButton* rDesk = new QRadioButton("Desktop (Kompakt)", grpUI);
    QRadioButton* rTab = new QRadioButton("Tablet (Groß)", grpUI);
    layUI->addWidget(rDesk); layUI->addWidget(rTab);
    rDesk->setChecked(true);
    contentLay->addWidget(grpUI);

    QButtonGroup* groupUI = new QButtonGroup(this);
    groupUI->addButton(rDesk, 0); groupUI->addButton(rTab, 1);
    connect(groupUI, &QButtonGroup::idClicked, [this](int id){ emit uiModeChanged(id == 1); });

    // 2. Toolbar
    QGroupBox* grpToolbar = new QGroupBox("Werkzeugleiste", contentWidget);
    grpToolbar->setStyleSheet("QGroupBox { border: 1px solid #333; border-radius: 8px; font-weight: bold; color: #888; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout* tbLay = new QVBoxLayout(grpToolbar);

    QRadioButton* rNorm = new QRadioButton("Normal (Adaptiv)", grpToolbar);
    rNorm->setObjectName("radioVert");
    QRadioButton* rFull = new QRadioButton("Radial (Voll)", grpToolbar);
    rFull->setObjectName("radioRadial");
    QRadioButton* rHalf = new QRadioButton("Radial (Halb)", grpToolbar);
    rHalf->setObjectName("radioRadialHalf");

    tbLay->addWidget(rNorm); tbLay->addWidget(rFull); tbLay->addWidget(rHalf);

    // Size Slider
    tbLay->addSpacing(10);
    tbLay->addWidget(new QLabel("Größe:", grpToolbar));
    QSlider* slSize = new QSlider(Qt::Horizontal, grpToolbar);
    slSize->setObjectName("sliderToolbarSize");
    slSize->setRange(50, 150); slSize->setValue(100);
    slSize->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(slSize, &QSlider::valueChanged, this, &SettingsDialog::toolbarScaleChanged);
    tbLay->addWidget(slSize);

    contentLay->addWidget(grpToolbar);

    QButtonGroup* bgToolbar = new QButtonGroup(this);
    bgToolbar->addButton(rNorm, 0); bgToolbar->addButton(rFull, 1); bgToolbar->addButton(rHalf, 2);
    connect(bgToolbar, &QButtonGroup::idClicked, [this](int id){ emit toolbarStyleChanged(id > 0); });

    // 3. Farben
    contentLay->addWidget(new QLabel("Akzentfarbe:", contentWidget));
    QWidget* colorW = new QWidget(contentWidget);
    QHBoxLayout* colorL = new QHBoxLayout(colorW);
    QList<QString> cNames = {"#2D62ED", "#5E5CE6", "#FF2D55", "#30D158"};
    for(auto c : cNames) {
        QPushButton* b = new QPushButton(colorW); b->setFixedSize(40,40);
        b->setStyleSheet(QString("background-color: %1; border-radius: 20px; border: 2px solid #333;").arg(c));
        connect(b, &QPushButton::clicked, [this, c](){ emit accentColorChanged(QColor(c)); });
        colorL->addWidget(b);
    }
    contentLay->addWidget(colorW);

    // 4. Grid
    contentLay->addWidget(new QLabel("Symbolgröße (Ordner):", contentWidget));
    QSlider* slItem = new QSlider(Qt::Horizontal, contentWidget);
    slItem->setObjectName("sliderItem"); slItem->setRange(60, 250);
    slItem->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(slItem, &QSlider::valueChanged, this, &SettingsDialog::itemSizeChanged);
    contentLay->addWidget(slItem);

    contentLay->addWidget(new QLabel("Gitterabstand:", contentWidget));
    QSlider* slSpace = new QSlider(Qt::Horizontal, contentWidget);
    slSpace->setObjectName("sliderSpace"); slSpace->setRange(0, 100);
    slSpace->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(slSpace, &QSlider::valueChanged, this, &SettingsDialog::gridSpacingChanged);
    contentLay->addWidget(slSpace);

    contentLay->addStretch();
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::setupColorButton(QPushButton* btn, QColor initialColor) { }
void SettingsDialog::onColorClicked() { }
void SettingsDialog::onColorButtonContextMenu(const QPoint &) { }

void SettingsDialog::setGridValues(int itemSize, int spacing) {
    QSlider* s1 = this->findChild<QSlider*>("sliderItem");
    QSlider* s2 = this->findChild<QSlider*>("sliderSpace");
    if(s1) { s1->blockSignals(true); s1->setValue(itemSize); s1->blockSignals(false); }
    if(s2) { s2->blockSignals(true); s2->setValue(spacing); s2->blockSignals(false); }
}

void SettingsDialog::setTouchMode(bool enabled) { }

void SettingsDialog::setToolbarConfig(bool isRadial, bool isHalf, int scalePercent) {
    QRadioButton* rVert = this->findChild<QRadioButton*>("radioVert");
    QRadioButton* rFull = this->findChild<QRadioButton*>("radioRadial");
    QRadioButton* rHalf = this->findChild<QRadioButton*>("radioRadialHalf");
    QSlider* sl = this->findChild<QSlider*>("sliderToolbarSize");

    if (sl) {
        sl->blockSignals(true);
        sl->setValue(scalePercent);
        sl->blockSignals(false);
    }

    if (isRadial) {
        if (isHalf && rHalf) rHalf->setChecked(true);
        else if (rFull) rFull->setChecked(true);
    } else if (rVert) {
        rVert->setChecked(true);
    }
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(200); anim->setStartValue(0.0); anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
