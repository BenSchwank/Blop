#include "settingsdialog.h"
#include "ui_SettingsDialog.h"
#include "UiProfileManager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QListWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    setupDesignTab();
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::setupDesignTab() {
    QWidget* tabDesign = ui->tabWidget->findChild<QWidget*>("tabDesign");
    if (!tabDesign) tabDesign = ui->tabWidget->widget(0);
    qDeleteAll(tabDesign->children());

    QVBoxLayout* mainLayout = new QVBoxLayout(tabDesign);
    mainLayout->setContentsMargins(20,20,20,20);
    mainLayout->setSpacing(15);

    // --- PROFIL LISTE ---
    QLabel* lbl = new QLabel("UI-Modi verwalten", tabDesign);
    lbl->setStyleSheet("font-weight: bold; color: #BBB; font-size: 14px;");
    mainLayout->addWidget(lbl);

    m_profileList = new QListWidget(tabDesign);
    m_profileList->setStyleSheet("QListWidget { background: transparent; border: 1px solid #333; border-radius: 5px; } QListWidget::item { border-bottom: 1px solid #222; } QListWidget::item:selected { background: #333; }");
    mainLayout->addWidget(m_profileList);

    QPushButton* btnAdd = new QPushButton("+ Neuen Modus erstellen", tabDesign);
    btnAdd->setStyleSheet("QPushButton { background-color: #333; color: white; border: 1px solid #444; border-radius: 5px; padding: 8px; } QPushButton:hover { background-color: #444; }");
    connect(btnAdd, &QPushButton::clicked, [this](){
        bool ok;
        QString text = QInputDialog::getText(this, "Neuer Modus", "Name:", QLineEdit::Normal, "Mein Modus", &ok);
        if (ok && !text.isEmpty()) {
            UiProfileManager::instance().addProfile(text);
            refreshProfileList();
        }
    });
    mainLayout->addWidget(btnAdd);

    refreshProfileList();

    // --- FARBEN ---
    mainLayout->addSpacing(20);
    mainLayout->addWidget(new QLabel("Akzentfarbe:", tabDesign));
    QWidget* colorW = new QWidget(tabDesign);
    QHBoxLayout* colorL = new QHBoxLayout(colorW);
    colorL->setContentsMargins(0,0,0,0);
    QList<QString> cNames = {"#2D62ED", "#5E5CE6", "#FF2D55", "#30D158"};
    for(auto c : cNames) {
        QPushButton* b = new QPushButton(colorW); b->setFixedSize(40,40);
        b->setStyleSheet(QString("background-color: %1; border-radius: 20px; border: 2px solid #333;").arg(c));
        connect(b, &QPushButton::clicked, [this, c](){ emit accentColorChanged(QColor(c)); });
        colorL->addWidget(b);
    }
    mainLayout->addWidget(colorW);
    mainLayout->addStretch();

    // --- EDITOR OVERLAY ---
    m_editorOverlay = new QWidget(this);
    m_editorOverlay->setStyleSheet("QWidget { background-color: #1A1A1A; border-radius: 15px; border: 1px solid #444; } QLabel { border: none; } QCheckBox { border: none; }");
    m_editorOverlay->hide();

    QVBoxLayout* ovMainLay = new QVBoxLayout(m_editorOverlay);
    ovMainLay->setContentsMargins(20,20,20,20);
    ovMainLay->setSpacing(10);

    QLabel* ovTitle = new QLabel("Modus bearbeiten", m_editorOverlay);
    ovTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white; margin-bottom: 5px;");
    ovMainLay->addWidget(ovTitle);

    QHBoxLayout* splitLay = new QHBoxLayout();

    // Linke Spalte
    QWidget* leftCol = new QWidget(m_editorOverlay);
    leftCol->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout* inputLay = new QVBoxLayout(leftCol);
    inputLay->setContentsMargins(0,0,10,0);

    auto addInput = [&](QString label, QSpinBox*& spin, int min, int max) {
        inputLay->addWidget(new QLabel(label, leftCol));
        spin = new QSpinBox(leftCol);
        spin->setRange(min, max);
        spin->setStyleSheet("QSpinBox { background: #333; color: white; padding: 5px; border: 1px solid #555; border-radius: 4px; }");
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::updatePreview);
        inputLay->addWidget(spin);
    };

    inputLay->addWidget(new QLabel("Name:", leftCol));
    edName = new QLineEdit(leftCol);
    edName->setStyleSheet("QLineEdit { background: #333; color: white; padding: 5px; border: 1px solid #555; border-radius: 4px; }");
    inputLay->addWidget(edName);

    addInput("Ordner Icon (px):", edFolderIcon, 8, 256);
    addInput("Button Icon (px):", edUiIcon, 8, 128); // NEU
    addInput("Button Größe (px):", edBtn, 20, 200);
    addInput("Raster-Item Breite (px):", edGridItem, 50, 400);
    addInput("Raster-Item Höhe (px):", edGridItemH, 50, 400); // NEU
    addInput("Raster-Abstand (px):", edGridSpace, 0, 100);

    edTouch = new QCheckBox("Touch-Optimiert", leftCol);
    edTouch->setStyleSheet("QCheckBox { color: #DDD; margin-top: 10px; } QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #555; border-radius: 4px; } QCheckBox::indicator:checked { background-color: #5E5CE6; }");
    connect(edTouch, &QCheckBox::toggled, this, &SettingsDialog::updatePreview);
    inputLay->addWidget(edTouch);

    inputLay->addStretch();

    splitLay->addWidget(leftCol, 1);

    // Rechte Spalte (Preview)
    m_preview = new PreviewWidget(m_editorOverlay);
    splitLay->addWidget(m_preview, 2);

    ovMainLay->addLayout(splitLay);

    QHBoxLayout* btnLay = new QHBoxLayout();
    QPushButton* btnCancel = new QPushButton("Abbrechen", m_editorOverlay);
    QPushButton* btnSave = new QPushButton("Speichern", m_editorOverlay);

    btnCancel->setCursor(Qt::PointingHandCursor);
    btnSave->setCursor(Qt::PointingHandCursor);

    btnCancel->setStyleSheet("background: #333; color: white; border: none; padding: 8px; border-radius: 5px;");
    btnSave->setStyleSheet("background: #5E5CE6; color: white; border: none; padding: 8px; border-radius: 5px; font-weight: bold;");

    connect(btnCancel, &QPushButton::clicked, [this](){ m_editorOverlay->hide(); });
    connect(btnSave, &QPushButton::clicked, this, &SettingsDialog::saveProfileFromEditor);

    btnLay->addWidget(btnCancel);
    btnLay->addWidget(btnSave);
    ovMainLay->addLayout(btnLay);
}

void SettingsDialog::updatePreview() {
    if (m_preview) {
        m_preview->updateValues(
            edFolderIcon->value(),
            edUiIcon->value(),
            edBtn->value(),
            edGridItem->value(),
            edGridItemH->value(),
            edGridSpace->value(),
            edTouch->isChecked()
            );
    }
}

QWidget* SettingsDialog::createProfileListItem(const UiProfile& p) {
    QWidget* w = new QWidget;
    QHBoxLayout* l = new QHBoxLayout(w);
    l->setContentsMargins(10, 5, 10, 5);

    QLabel* name = new QLabel(p.name);
    name->setStyleSheet("font-size: 14px; color: white; border: none;");
    if (p.id == UiProfileManager::instance().currentProfile().id) {
        name->setStyleSheet("font-size: 14px; color: #5E5CE6; font-weight: bold; border: none;");
        name->setText(p.name + " (Aktiv)");
    }
    l->addWidget(name);
    l->addStretch();

    QPushButton* menuBtn = new QPushButton("⋮");
    menuBtn->setFixedSize(30, 30);
    menuBtn->setStyleSheet("QPushButton { background: transparent; color: #AAA; font-size: 18px; border: none; font-weight: bold; } QPushButton:hover { color: white; background-color: #333; border-radius: 15px; }");
    menuBtn->setCursor(Qt::PointingHandCursor);

    connect(menuBtn, &QPushButton::clicked, [this, p, menuBtn](){
        QMenu menu;
        menu.setStyleSheet("QMenu { background-color: #252526; color: white; border: 1px solid #444; } QMenu::item { padding: 5px 20px; } QMenu::item:selected { background-color: #5E5CE6; }");

        menu.addAction("Bearbeiten", [this, p](){ showProfileEditor(p); });

        menu.addAction("Umbenennen", [this, p](){
            bool ok;
            QString text = QInputDialog::getText(this, "Umbenennen", "Neuer Name:", QLineEdit::Normal, p.name, &ok);
            if (ok && !text.isEmpty()) {
                UiProfile copy = p; copy.name = text;
                UiProfileManager::instance().updateProfile(copy);
                refreshProfileList();
            }
        });

        QAction* delAct = menu.addAction("Löschen");
        if (UiProfileManager::instance().profiles().size() <= 1) delAct->setEnabled(false);

        connect(delAct, &QAction::triggered, [this, p](){
            if (QMessageBox::question(this, "Löschen", "Profil wirklich löschen?") == QMessageBox::Yes) {
                UiProfileManager::instance().removeProfile(p.id);
                refreshProfileList();
            }
        });

        menu.exec(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    l->addWidget(menuBtn);
    return w;
}

void SettingsDialog::refreshProfileList() {
    m_profileList->clear();
    auto profiles = UiProfileManager::instance().profiles();
    for(const auto& p : profiles) {
        QListWidgetItem* item = new QListWidgetItem(m_profileList);
        item->setSizeHint(QSize(0, 50));
        QWidget* widget = createProfileListItem(p);
        m_profileList->setItemWidget(item, widget);
    }
}

void SettingsDialog::showProfileEditor(const UiProfile& p) {
    m_editingId = p.id;
    edName->setText(p.name);

    bool oldState = edFolderIcon->blockSignals(true);
    edFolderIcon->setValue(p.folderIconSize);
    edUiIcon->setValue(p.uiIconSize); // NEU
    edBtn->setValue(p.buttonSize);
    edGridItem->setValue(p.gridItemSize);
    edGridItemH->setValue(p.gridItemHeight); // NEU
    edGridSpace->setValue(p.gridSpacing);
    edTouch->setChecked(p.isTouchOptimized);
    edFolderIcon->blockSignals(oldState);

    updatePreview();

    m_editorOverlay->resize(width() - 40, height() - 40);
    m_editorOverlay->move(20, 20);
    m_editorOverlay->raise();
    m_editorOverlay->show();
}

void SettingsDialog::saveProfileFromEditor() {
    auto profiles = UiProfileManager::instance().profiles();
    for(const auto& existing : profiles) {
        if (existing.id == m_editingId) {
            UiProfile copy = existing;
            copy.name = edName->text();
            copy.folderIconSize = edFolderIcon->value();
            copy.uiIconSize = edUiIcon->value();
            copy.buttonSize = edBtn->value();
            copy.gridItemSize = edGridItem->value();
            copy.gridItemHeight = edGridItemH->value();
            copy.gridSpacing = edGridSpace->value();
            copy.isTouchOptimized = edTouch->isChecked();

            UiProfileManager::instance().updateProfile(copy);
            break;
        }
    }
    m_editorOverlay->hide();
    refreshProfileList();
}

void SettingsDialog::setToolbarConfig(bool isRadial, bool isHalf, int scalePercent) {
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(200); anim->setStartValue(0.0); anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
