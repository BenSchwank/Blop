#include "settingsdialog.h"
#include "ui_SettingsDialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QScrollArea>
#include <QGroupBox>
#include <QButtonGroup>
#include <QRadioButton>
#include <QLineEdit>

SettingsDialog::SettingsDialog(UiProfileManager* profileMgr, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog),
    m_profileManager(profileMgr)
{
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    QWidget* tabDesign = ui->tabWidget->widget(0);
    if (tabDesign) {
        qDeleteAll(tabDesign->children());
        if (tabDesign->layout()) delete tabDesign->layout();
    }

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

    QGroupBox* grpProfiles = new QGroupBox("UI-Modi (Profile)", contentWidget);
    grpProfiles->setStyleSheet("QGroupBox { border: 1px solid #333; border-radius: 8px; font-weight: bold; color: #888; padding-top: 15px; }");
    QVBoxLayout* layProfiles = new QVBoxLayout(grpProfiles);

    m_profileList = new QListWidget(grpProfiles);
    m_profileList->setStyleSheet("QListWidget { background: #222; border: 1px solid #333; border-radius: 5px; color: white; } QListWidget::item { padding: 8px; } QListWidget::item:selected { background: #5E5CE6; }");
    m_profileList->setFixedHeight(120);
    m_profileList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_profileList, &QListWidget::customContextMenuRequested, this, &SettingsDialog::onProfileContextMenu);
    connect(m_profileList, &QListWidget::itemClicked, this, &SettingsDialog::onProfileClicked);

    layProfiles->addWidget(m_profileList);

    QPushButton* btnNewProfile = new QPushButton("Neuen Modus erstellen", grpProfiles);
    btnNewProfile->setCursor(Qt::PointingHandCursor);
    connect(btnNewProfile, &QPushButton::clicked, this, &SettingsDialog::onCreateProfile);
    layProfiles->addWidget(btnNewProfile);

    contentLay->addWidget(grpProfiles);

    QGroupBox* grpToolbar = new QGroupBox("Werkzeugleiste Stil", contentWidget);
    grpToolbar->setStyleSheet("QGroupBox { border: 1px solid #333; border-radius: 8px; font-weight: bold; color: #888; padding-top: 15px; }");
    QVBoxLayout* tbLay = new QVBoxLayout(grpToolbar);

    QRadioButton* rNorm = new QRadioButton("Vertikal / Adaptiv", grpToolbar);
    rNorm->setObjectName("radioVert");
    QRadioButton* rFull = new QRadioButton("Radial (Voll)", grpToolbar);
    rFull->setObjectName("radioRadial");
    QRadioButton* rHalf = new QRadioButton("Radial (Halb)", grpToolbar);
    rHalf->setObjectName("radioRadialHalf");

    tbLay->addWidget(rNorm); tbLay->addWidget(rFull); tbLay->addWidget(rHalf);
    contentLay->addWidget(grpToolbar);

    QButtonGroup* bgToolbar = new QButtonGroup(this);
    bgToolbar->addButton(rNorm, 0); bgToolbar->addButton(rFull, 1); bgToolbar->addButton(rHalf, 2);
    connect(bgToolbar, &QButtonGroup::idClicked, [this](int id){ emit toolbarStyleChanged(id > 0); });

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
    contentLay->addStretch();

    refreshProfileList();
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    QPropertyAnimation *anim = new QPropertyAnimation(this, "windowOpacity");
    anim->setDuration(200); anim->setStartValue(0.0); anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void SettingsDialog::refreshProfileList() {
    m_profileList->clear();
    QString currentId = m_profileManager->currentProfile().id;
    for(const auto& p : m_profileManager->profiles()) {
        QListWidgetItem* item = new QListWidgetItem(p.name);
        item->setData(Qt::UserRole, p.id);
        m_profileList->addItem(item);
        if (p.id == currentId) m_profileList->setCurrentItem(item);
    }
}

void SettingsDialog::onProfileClicked(QListWidgetItem* item) {
    if(!item) return;
    QString id = item->data(Qt::UserRole).toString();
    m_profileManager->setCurrentProfile(id);
}

void SettingsDialog::onCreateProfile() {
    bool ok;
    QString text = QInputDialog::getText(this, "Neuer Modus", "Name:", QLineEdit::Normal, "Mein Modus", &ok);
    if (ok && !text.isEmpty()) {
        m_profileManager->createProfile(text);
        refreshProfileList();
    }
}

void SettingsDialog::onProfileContextMenu(const QPoint &pos) {
    QListWidgetItem *item = m_profileList->itemAt(pos);
    if (!item) return;
    QMenu menu(this);
    menu.addAction("Bearbeiten", [this, item](){ openEditor(item->data(Qt::UserRole).toString()); });
    menu.addAction("Umbenennen", [this, item](){
        bool ok;
        QString text = QInputDialog::getText(this, "Umbenennen", "Name:", QLineEdit::Normal, item->text(), &ok);
        if(ok && !text.isEmpty()) {
            UiProfile p = m_profileManager->profileById(item->data(Qt::UserRole).toString());
            p.name = text;
            m_profileManager->updateProfile(p);
            refreshProfileList();
        }
    });
    menu.addAction("Löschen", [this, item](){
        if(QMessageBox::question(this, "Löschen", "Modus wirklich löschen?") == QMessageBox::Yes) {
            m_profileManager->deleteProfile(item->data(Qt::UserRole).toString());
            refreshProfileList();
        }
    });
    menu.exec(m_profileList->mapToGlobal(pos));
}

void SettingsDialog::openEditor(const QString &profileId) {
    m_editId = profileId;
    // Wir schließen den Dialog mit einem speziellen Code.
    // MainWindow fängt diesen ab und öffnet den Editor.
    // Das verhindert Modality-Probleme und Freezes.
    done(EditProfileCode);
}

void SettingsDialog::setToolbarConfig(bool isRadial, bool isHalf) {
    QRadioButton* rVert = this->findChild<QRadioButton*>("radioVert");
    QRadioButton* rFull = this->findChild<QRadioButton*>("radioRadial");
    QRadioButton* rHalf = this->findChild<QRadioButton*>("radioRadialHalf");
    if (isRadial) {
        if (isHalf && rHalf) rHalf->setChecked(true);
        else if (rFull) rFull->setChecked(true);
    } else if (rVert) {
        rVert->setChecked(true);
    }
}
