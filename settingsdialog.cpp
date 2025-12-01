#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPropertyAnimation>
#include <QSlider>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    connect(ui->btnColorBlue, &QPushButton::clicked, this, &SettingsDialog::onColorClicked);
    connect(ui->btnColorPurple, &QPushButton::clicked, this, &SettingsDialog::onColorClicked);
    connect(ui->btnColorPink, &QPushButton::clicked, this, &SettingsDialog::onColorClicked);
    connect(ui->btnColorGreen, &QPushButton::clicked, this, &SettingsDialog::onColorClicked);

    // --- WIDGETS IN DEN DESIGN-TAB EINFÜGEN ---

    // Wir suchen das Layout vom "Design"-Tab
    QWidget* targetTab = ui->tabWidget->findChild<QWidget*>("tabDesign");
    if (!targetTab) targetTab = ui->tabWidget->widget(0);

    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(targetTab->layout());
    if (!layout) layout = new QVBoxLayout(targetTab);

    // Position vor dem Spacer finden (letztes Item)
    int insertPos = layout->count() > 0 ? layout->count() - 1 : 0;

    // --- SLIDER 1: SYMBOLGRÖSSE ---
    layout->insertSpacing(insertPos, 20); insertPos++;

    QLabel* lblSize = new QLabel("Symbolgröße", targetTab);
    lblSize->setStyleSheet("color: #E0E0E0; font-weight: bold; font-size: 14px;");
    layout->insertWidget(insertPos, lblSize); insertPos++;

    QSlider* sliderItem = new QSlider(Qt::Horizontal, targetTab);
    sliderItem->setObjectName("sliderItem");
    sliderItem->setRange(60, 250);
    sliderItem->setCursor(Qt::PointingHandCursor);
    sliderItem->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    layout->insertWidget(insertPos, sliderItem); insertPos++;

    connect(sliderItem, &QSlider::valueChanged, this, &SettingsDialog::itemSizeChanged);

    // --- SLIDER 2: ABSTAND ---
    QLabel* lblSpace = new QLabel("Gitterabstand", targetTab);
    lblSpace->setStyleSheet("color: #E0E0E0; font-weight: bold; font-size: 14px; margin-top: 5px;");
    layout->insertWidget(insertPos, lblSpace); insertPos++;

    QSlider* sliderSpace = new QSlider(Qt::Horizontal, targetTab);
    sliderSpace->setObjectName("sliderSpace");
    sliderSpace->setRange(0, 100);
    sliderSpace->setCursor(Qt::PointingHandCursor);
    sliderSpace->setStyleSheet("QSlider::groove:horizontal { border: 1px solid #333; height: 6px; background: #1A1A1A; margin: 2px 0; border-radius: 3px; } QSlider::handle:horizontal { background: #5E5CE6; border: 1px solid #5E5CE6; width: 16px; height: 16px; margin: -6px 0; border-radius: 8px; }");
    layout->insertWidget(insertPos, sliderSpace); insertPos++;

    connect(sliderSpace, &QSlider::valueChanged, this, &SettingsDialog::gridSpacingChanged);

    // --- UI MODUS (Existierende Buttons nutzen) ---
    QButtonGroup* group = new QButtonGroup(this);
    group->addButton(ui->radioDesktop, 0);
    group->addButton(ui->radioTablet, 1);

    connect(group, &QButtonGroup::idClicked, [this](int id){
        emit uiModeChanged(id == 1);
    });
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

// WIEDER HINZUGEFÜGT: Setter für die Slider
void SettingsDialog::setGridValues(int itemSize, int spacing) {
    QSlider* s1 = this->findChild<QSlider*>("sliderItem");
    QSlider* s2 = this->findChild<QSlider*>("sliderSpace");
    if(s1) {
        s1->blockSignals(true);
        s1->setValue(itemSize);
        s1->blockSignals(false);
    }
    if(s2) {
        s2->blockSignals(true);
        s2->setValue(spacing);
        s2->blockSignals(false);
    }
}

void SettingsDialog::setTouchMode(bool enabled) {
    if (enabled) ui->radioTablet->setChecked(true);
    else ui->radioDesktop->setChecked(true);
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(200);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SettingsDialog::onColorClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    QColor color;
    if (btn == ui->btnColorBlue) color = QColor(0x2D62ED);
    else if (btn == ui->btnColorPurple) color = QColor(0x5E5CE6);
    else if (btn == ui->btnColorPink) color = QColor(0xFF2D55);
    else if (btn == ui->btnColorGreen) color = QColor(0x30D158);

    emit accentColorChanged(color);
}
