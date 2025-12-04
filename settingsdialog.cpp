#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QSlider>
#include <QColorDialog>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    setupColorButton(ui->btnColorBlue, QColor(0x2D62ED));
    setupColorButton(ui->btnColorPurple, QColor(0x5E5CE6));
    setupColorButton(ui->btnColorPink, QColor(0xFF2D55));
    setupColorButton(ui->btnColorGreen, QColor(0x30D158));

    QWidget* targetTab = ui->tabWidget->findChild<QWidget*>("tabDesign");
    if (!targetTab) targetTab = ui->tabWidget->widget(0);

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(targetTab->layout());
    if (!layout) layout = new QVBoxLayout(targetTab);

    int insertPos = layout->count() > 0 ? layout->count() - 1 : 0;

    // --- TOOLBAR STYLE ---
    QGroupBox* grpToolbar = new QGroupBox("Werkzeugleiste", targetTab);
    grpToolbar->setStyleSheet("QGroupBox { border: 1px solid #333; border-radius: 8px; margin-top: 20px; font-weight: bold; color: #888; padding-top: 15px; } QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }");
    QVBoxLayout* tbLayout = new QVBoxLayout(grpToolbar);

    QRadioButton* radioVert = new QRadioButton("Vertikal", grpToolbar);
    QRadioButton* radioRadial = new QRadioButton("Radial (Voll)", grpToolbar);
    QRadioButton* radioRadialHalf = new QRadioButton("Radial (Halb)", grpToolbar);

    radioVert->setObjectName("radioVert");
    radioRadial->setObjectName("radioRadial");
    radioRadialHalf->setObjectName("radioRadialHalf");

    radioVert->setStyleSheet("color: #E0E0E0; spacing: 10px;");
    radioRadial->setStyleSheet("color: #E0E0E0; spacing: 10px;");
    radioRadialHalf->setStyleSheet("color: #E0E0E0; spacing: 10px;");

    QButtonGroup* bgToolbar = new QButtonGroup(this);
    bgToolbar->addButton(radioVert, 0);
    bgToolbar->addButton(radioRadial, 1);
    bgToolbar->addButton(radioRadialHalf, 2);

    connect(bgToolbar, &QButtonGroup::idClicked, [this](int id){
        emit toolbarStyleChanged(id > 0);
    });

    tbLayout->addWidget(radioVert);
    tbLayout->addWidget(radioRadial);
    tbLayout->addWidget(radioRadialHalf);

    // NEU: Größe Slider für Toolbar
    tbLayout->addSpacing(10);
    tbLayout->addWidget(new QLabel("Größe:", grpToolbar));
    QSlider* slSize = new QSlider(Qt::Horizontal, grpToolbar);
    slSize->setObjectName("sliderToolbarSize");
    slSize->setRange(50, 150); // 50% bis 150%
    slSize->setValue(100);
    slSize->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    connect(slSize, &QSlider::valueChanged, this, &SettingsDialog::toolbarScaleChanged);
    tbLayout->addWidget(slSize);

    layout->insertWidget(insertPos, grpToolbar); insertPos++;
    layout->insertSpacing(insertPos, 10); insertPos++;

    // ... (Rest wie bisher)
    // Hinweis Label
    QLabel* lblHint = new QLabel("(Rechtsklick auf Farbe zum Ändern)", targetTab);
    lblHint->setStyleSheet("color: #888; font-size: 11px; font-style: italic; margin-bottom: 5px;");
    lblHint->setAlignment(Qt::AlignCenter);
    layout->insertWidget(insertPos, lblHint); insertPos++;
    layout->insertSpacing(insertPos, 10); insertPos++;

    QPushButton* btnCustom = new QPushButton("🎨 Einmalige Farbe wählen...", targetTab);
    btnCustom->setCursor(Qt::PointingHandCursor);
    btnCustom->setStyleSheet("QPushButton { background-color: #252526; color: #E0E0E0; border: 1px solid #333; border-radius: 8px; padding: 10px; font-weight: bold; text-align: left; } QPushButton:hover { background-color: #333; border-color: #555; }");
    connect(btnCustom, &QPushButton::clicked, [this](){
        QColor color = QColorDialog::getColor(QColor(0x5E5CE6), this, "Farbe wählen");
        if (color.isValid()) emit accentColorChanged(color);
    });
    layout->insertWidget(insertPos, btnCustom); insertPos++;

    layout->insertSpacing(insertPos, 20); insertPos++;
    QLabel* lblSize = new QLabel("Symbolgröße (Ordner)", targetTab);
    lblSize->setStyleSheet("color: #E0E0E0; font-weight: bold; font-size: 14px;");
    layout->insertWidget(insertPos, lblSize); insertPos++;

    QSlider* sliderItem = new QSlider(Qt::Horizontal, targetTab);
    sliderItem->setObjectName("sliderItem");
    sliderItem->setRange(60, 250);
    sliderItem->setCursor(Qt::PointingHandCursor);
    sliderItem->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    layout->insertWidget(insertPos, sliderItem); insertPos++;
    connect(sliderItem, &QSlider::valueChanged, this, &SettingsDialog::itemSizeChanged);

    QLabel* lblSpace = new QLabel("Gitterabstand (Ordner)", targetTab);
    lblSpace->setStyleSheet("color: #E0E0E0; font-weight: bold; font-size: 14px; margin-top: 5px;");
    layout->insertWidget(insertPos, lblSpace); insertPos++;

    QSlider* sliderSpace = new QSlider(Qt::Horizontal, targetTab);
    sliderSpace->setObjectName("sliderSpace");
    sliderSpace->setRange(0, 100);
    sliderSpace->setCursor(Qt::PointingHandCursor);
    sliderSpace->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    layout->insertWidget(insertPos, sliderSpace); insertPos++;
    connect(sliderSpace, &QSlider::valueChanged, this, &SettingsDialog::gridSpacingChanged);

    QButtonGroup* group = new QButtonGroup(this);
    group->addButton(ui->radioDesktop, 0);
    group->addButton(ui->radioTablet, 1);
    connect(group, &QButtonGroup::idClicked, [this](int id){ emit uiModeChanged(id == 1); });
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::setupColorButton(QPushButton* btn, QColor initialColor) {
    btn->setProperty("customColor", initialColor);
    btn->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(btn, &QPushButton::clicked, this, &SettingsDialog::onColorClicked);
    connect(btn, &QWidget::customContextMenuRequested, this, &SettingsDialog::onColorButtonContextMenu);
    btn->setStyleSheet(QString("background-color: %1; border-radius: 25px; border: 2px solid #1A1A1A;").arg(initialColor.name()));
}

void SettingsDialog::onColorClicked() {
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QColor color = btn->property("customColor").value<QColor>();
    if (color.isValid()) emit accentColorChanged(color);
}

void SettingsDialog::onColorButtonContextMenu(const QPoint &) {
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QColor currentColor = btn->property("customColor").value<QColor>();
    QColor newColor = QColorDialog::getColor(currentColor, this, "Slot-Farbe ändern");
    if (newColor.isValid()) {
        btn->setProperty("customColor", newColor);
        btn->setStyleSheet(QString("background-color: %1; border-radius: 25px; border: 2px solid #1A1A1A;").arg(newColor.name()));
        emit accentColorChanged(newColor);
    }
}

void SettingsDialog::setGridValues(int itemSize, int spacing) {
    QSlider* s1 = this->findChild<QSlider*>("sliderItem");
    QSlider* s2 = this->findChild<QSlider*>("sliderSpace");
    if(s1) { s1->blockSignals(true); s1->setValue(itemSize); s1->blockSignals(false); }
    if(s2) { s2->blockSignals(true); s2->setValue(spacing); s2->blockSignals(false); }
}

void SettingsDialog::setTouchMode(bool enabled) {
    if (enabled) ui->radioTablet->setChecked(true);
    else ui->radioDesktop->setChecked(true);
}

void SettingsDialog::setToolbarConfig(bool isRadial, bool isHalf, int scalePercent) {
    QRadioButton* rVert = this->findChild<QRadioButton*>("radioVert");
    QRadioButton* rFull = this->findChild<QRadioButton*>("radioRadial");
    QRadioButton* rHalf = this->findChild<QRadioButton*>("radioRadialHalf");
    QSlider* sl = this->findChild<QSlider*>("sliderToolbarSize");

    if (sl) { sl->blockSignals(true); sl->setValue(scalePercent); sl->blockSignals(false); }

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
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
