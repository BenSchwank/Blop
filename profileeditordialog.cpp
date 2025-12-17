#include "profileeditordialog.h"
#include "moderntoolbar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QMouseEvent>
#include <QButtonGroup>
#include <cmath>

ProfileEditorDialog::ProfileEditorDialog(UiProfile profile, QWidget *parent)
    : QDialog(parent), m_profile(profile), m_previewToolbar(nullptr)
{
    setWindowTitle("Profil bearbeiten");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    if (parent) {
        resize(650, 600); // Etwas breiter für die 5 Buttons
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        move(parentCenter.x() - width() / 2, parentCenter.y() - height() / 2);
    }

    setStyleSheet("QDialog { background-color: #1E1E1E; border: 1px solid #555; border-radius: 12px; }"
                  "QLabel { color: #DDD; font-weight: bold; border: none; background: transparent; }"
                  "QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }"
                  "QSlider::handle:horizontal { background: #5E5CE6; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; }"
                  "QPushButton { background: #333; color: white; border: 1px solid #555; padding: 8px 16px; border-radius: 5px; }"
                  "QPushButton:hover { background: #444; }"
                  "QCheckBox { color: #DDD; background: transparent; }"
                  "QGroupBox { border: 1px solid #333; border-radius: 5px; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #888; }");

    setupUi();

    if (m_previewToolbar) {
        m_previewToolbar->setScale(m_profile.toolbarScale);
    }

    m_simpleContainer->setVisible(m_isSimpleMode);
    m_complexContainer->setVisible(!m_isSimpleMode);

    updateSimpleFromComplex();
}

void ProfileEditorDialog::setupUi() {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // --- LINKE SEITE ---
    QWidget *leftWidget = new QWidget(this);
    leftWidget->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(10);

    // Header
    QHBoxLayout *header = new QHBoxLayout;
    QLabel *title = new QLabel("Profil bearbeiten: " + m_profile.name, this);
    title->setStyleSheet("font-size: 18px; color: #5E5CE6;");

    m_btnToggleMode = new QPushButton("Experten-Modus", this);
    m_btnToggleMode->setCursor(Qt::PointingHandCursor);
    m_btnToggleMode->setStyleSheet("QPushButton { font-size: 12px; padding: 4px 10px; background: #2D2D30; color: #BBB; border-radius: 12px; } QPushButton:hover { color: white; background: #3E3E42; }");
    connect(m_btnToggleMode, &QPushButton::clicked, this, &ProfileEditorDialog::toggleMode);

    header->addWidget(title);
    header->addStretch();
    header->addWidget(m_btnToggleMode);
    leftLayout->addLayout(header);

    // ---------------------------------------------------------
    // CONTAINER 1: Einfacher Modus (5 Buttons)
    // ---------------------------------------------------------
    m_simpleContainer = new QWidget(this);
    QVBoxLayout *simpleLay = new QVBoxLayout(m_simpleContainer);
    simpleLay->setContentsMargins(0, 20, 0, 0);
    simpleLay->setSpacing(20);

    QLabel *lblInfo = new QLabel("Wähle eine Größe für die Benutzeroberfläche:", m_simpleContainer);
    lblInfo->setStyleSheet("color: #BBB; font-weight: normal; font-size: 14px;");
    simpleLay->addWidget(lblInfo);

    simpleLay->addStretch();

    // Button Group
    m_btnGroupSimple = new QButtonGroup(this);
    m_btnGroupSimple->setExclusive(true);
    connect(m_btnGroupSimple, &QButtonGroup::idClicked, this, &ProfileEditorDialog::onSimpleStepClicked);

    QHBoxLayout *hSteps = new QHBoxLayout;
    hSteps->setSpacing(10);

    // Labels für die 5 Schritte
    QStringList labels = {"Sehr Klein", "Klein", "Normal", "Groß", "Sehr Groß"};

    for(int i=0; i<5; ++i) {
        QPushButton *b = new QPushButton(labels[i]);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(50);

        // Spezielles Styling für Toggle-Verhalten
        b->setStyleSheet(
            "QPushButton { background: #252526; color: #AAA; border: 1px solid #444; border-radius: 8px; font-weight: bold; }"
            "QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }"
            "QPushButton:hover:!checked { background: #333; border: 1px solid #555; }"
            );

        m_btnGroupSimple->addButton(b, i);
        hSteps->addWidget(b);
    }
    simpleLay->addLayout(hSteps);

    // Erklärungstext unter den Buttons (optional, ändert sich vielleicht je nach Auswahl)
    QLabel *lblHint = new QLabel("Dies passt Icons, Buttons und Abstände automatisch an.", m_simpleContainer);
    lblHint->setStyleSheet("color: #666; font-style: italic; font-size: 12px; margin-top: 10px;");
    lblHint->setAlignment(Qt::AlignCenter);
    simpleLay->addWidget(lblHint);

    simpleLay->addStretch();

    // ---------------------------------------------------------
    // CONTAINER 2: Komplexer Modus
    // ---------------------------------------------------------
    m_complexContainer = new QWidget(this);
    QVBoxLayout *complexLay = new QVBoxLayout(m_complexContainer);
    complexLay->setContentsMargins(0, 10, 0, 0);
    complexLay->setSpacing(15);

    auto addSlider = [this, complexLay](QString text, int min, int max, int val, QSlider*& outSl, QLabel*& outLbl) {
        complexLay->addWidget(new QLabel(text));
        QHBoxLayout *h = new QHBoxLayout;
        outSl = new QSlider(Qt::Horizontal);
        outSl->setRange(min, max);
        outSl->setValue(val);
        outLbl = new QLabel(QString::number(val));
        outLbl->setFixedWidth(50);
        outLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(outSl);
        h->addWidget(outLbl);
        complexLay->addLayout(h);
        connect(outSl, &QSlider::valueChanged, this, &ProfileEditorDialog::onValuesChanged);
    };

    addSlider("Element Größe (px)", 40, 500, m_profile.iconSize, m_slIconSize, m_lblIconVal);
    addSlider("Button / Touch Target (px)", 24, 80, m_profile.buttonSize, m_slButtonSize, m_lblBtnVal);
    addSlider("Toolbar Skalierung (%)", 50, 200, (int)(m_profile.toolbarScale * 100), m_slToolbarScale, m_lblToolbarVal);
    m_lblToolbarVal->setText(QString::number(m_slToolbarScale->value()) + "%");
    addSlider("Raster Abstand (px)", 0, 100, m_profile.gridSpacing, m_slGridSpace, m_lblGridVal);

    m_chkSnap = new QCheckBox("Am Raster ausrichten", this);
    m_chkSnap->setChecked(m_profile.snapToGrid);
    connect(m_chkSnap, &QCheckBox::toggled, this, &ProfileEditorDialog::onValuesChanged);
    complexLay->addWidget(m_chkSnap);

    complexLay->addStretch();

    leftLayout->addWidget(m_simpleContainer);
    leftLayout->addWidget(m_complexContainer);

    // Buttons unten
    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *btnCancel = new QPushButton("Abbrechen", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton *btnSave = new QPushButton("Speichern", this);
    btnSave->setStyleSheet("background-color: #5E5CE6; border: none; font-weight: bold;");
    connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
    btns->addWidget(btnCancel);
    btns->addWidget(btnSave);
    leftLayout->addLayout(btns);

    // --- RECHTE SEITE ---
    m_previewBox = new QGroupBox("Vorschau (Radial)", this);
    m_previewBox->setFixedWidth(200);
    m_previewBox->setStyleSheet("QGroupBox { background: #252526; border: 1px solid #444; }");

    m_previewToolbar = new ModernToolbar(m_previewBox);
    m_previewToolbar->setPreviewMode(true);
    m_previewToolbar->setStyle(ModernToolbar::Radial);
    m_previewToolbar->setRadialType(ModernToolbar::FullCircle);
    m_previewToolbar->setDraggable(false);
    m_previewToolbar->show();

    mainLayout->addWidget(leftWidget, 1);
    mainLayout->addWidget(m_previewBox, 0);
}

void ProfileEditorDialog::toggleMode() {
    m_isSimpleMode = !m_isSimpleMode;
    m_simpleContainer->setVisible(m_isSimpleMode);
    m_complexContainer->setVisible(!m_isSimpleMode);

    if (m_isSimpleMode) {
        m_btnToggleMode->setText("Experten-Modus");
        updateSimpleFromComplex();
    } else {
        m_btnToggleMode->setText("Einfacher Modus");
    }
}

// Logik für die 5 Buttons
void ProfileEditorDialog::onSimpleStepClicked(int id) {
    if (id < 0 || id >= m_stepFactors.size()) return;

    double factor = m_stepFactors[id];

    // Basiswerte (entsprechen "Normal" / Faktor 1.0)
    const int baseIcon = 100;
    const int baseButton = 40;
    const int baseGrid = 20;
    const double baseToolbar = 1.0;

    m_profile.iconSize = (int)(baseIcon * factor);
    m_profile.buttonSize = (int)(baseButton * factor);
    m_profile.gridSpacing = (int)(baseGrid * factor);
    m_profile.toolbarScale = baseToolbar * factor;

    // Slider synchronisieren (ohne doppelte Signale)
    bool oldBlock = m_slIconSize->blockSignals(true);
    m_slIconSize->setValue(m_profile.iconSize);
    m_slButtonSize->setValue(m_profile.buttonSize);
    m_slGridSpace->setValue(m_profile.gridSpacing);
    m_slToolbarScale->setValue((int)(m_profile.toolbarScale * 100));
    m_slIconSize->blockSignals(oldBlock);

    // Labels aktualisieren
    m_lblIconVal->setText(QString::number(m_profile.iconSize));
    m_lblBtnVal->setText(QString::number(m_profile.buttonSize));
    m_lblGridVal->setText(QString::number(m_profile.gridSpacing));
    m_lblToolbarVal->setText(QString::number((int)(m_profile.toolbarScale * 100)) + "%");

    // Vorschau aktualisieren
    if (m_previewToolbar) {
        m_previewToolbar->setScale(m_profile.toolbarScale);
        int cx = m_previewBox->width() / 2;
        int cy = m_previewBox->height() / 2;
        m_previewToolbar->move(cx - m_previewToolbar->width()/2, cy - m_previewToolbar->height()/2);
    }

    emit previewRequested(m_profile);
}

void ProfileEditorDialog::updateSimpleFromComplex() {
    // Wir versuchen herauszufinden, welcher der 5 Schritte am ehesten passt.
    // Wir nehmen "Toolbar Scale" als Indikator.
    double currentScale = m_profile.toolbarScale;

    int bestIdx = 2; // Default "Normal"
    double minDiff = 100.0;

    for(int i=0; i<m_stepFactors.size(); ++i) {
        double diff = std::abs(currentScale - m_stepFactors[i]);
        if (diff < minDiff) {
            minDiff = diff;
            bestIdx = i;
        }
    }

    // Button als gecheckt markieren (Signal blockieren, damit wir nicht rekursiv Werte setzen)
    m_btnGroupSimple->blockSignals(true);
    if(auto btn = m_btnGroupSimple->button(bestIdx)) {
        btn->setChecked(true);
    }
    m_btnGroupSimple->blockSignals(false);
}

// Logik für Komplexen Modus
void ProfileEditorDialog::onValuesChanged() {
    m_profile.iconSize = m_slIconSize->value();
    m_profile.buttonSize = m_slButtonSize->value();
    m_profile.gridSpacing = m_slGridSpace->value();
    m_profile.toolbarScale = m_slToolbarScale->value() / 100.0;
    m_profile.snapToGrid = m_chkSnap->isChecked();

    m_lblIconVal->setText(QString::number(m_profile.iconSize));
    m_lblBtnVal->setText(QString::number(m_profile.buttonSize));
    m_lblGridVal->setText(QString::number(m_profile.gridSpacing));
    m_lblToolbarVal->setText(QString::number(m_slToolbarScale->value()) + "%");

    if (m_previewToolbar) {
        m_previewToolbar->setScale(m_profile.toolbarScale);
        int cx = m_previewBox->width() / 2;
        int cy = m_previewBox->height() / 2;
        m_previewToolbar->move(cx - m_previewToolbar->width()/2, cy - m_previewToolbar->height()/2);
    }

    emit previewRequested(m_profile);
}

void ProfileEditorDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ProfileEditorDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}
