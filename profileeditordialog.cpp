#include "ProfileEditorDialog.h"
#include "moderntoolbar.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QMouseEvent>

ProfileEditorDialog::ProfileEditorDialog(UiProfile profile, QWidget *parent)
    : QDialog(parent), m_profile(profile), m_previewToolbar(nullptr)
{
    setWindowTitle("Profil bearbeiten");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setStyleSheet("QDialog { background-color: #1E1E1E; border: 1px solid #555; border-radius: 12px; }"
                  "QLabel { color: #DDD; font-weight: bold; border: none; background: transparent; }"
                  "QSlider::groove:horizontal { height: 6px; background: #333; border-radius: 3px; }"
                  "QSlider::handle:horizontal { background: #5E5CE6; width: 16px; height: 16px; margin: -5px 0; border-radius: 8px; }"
                  "QPushButton { background: #333; color: white; border: 1px solid #555; padding: 8px 16px; border-radius: 5px; }"
                  "QPushButton:hover { background: #444; }"
                  "QCheckBox { color: #DDD; background: transparent; }"
                  "QGroupBox { border: 1px solid #333; border-radius: 5px; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #888; }");

    if (parent) {
        resize(580, 550);
        QPoint parentCenter = parent->mapToGlobal(parent->rect().center());
        move(parentCenter.x() - width() / 2, parentCenter.y() - height() / 2);
    }

    setupUi();

    if (m_previewToolbar) {
        m_previewToolbar->setScale(m_profile.toolbarScale);
    }
}

void ProfileEditorDialog::setupUi() {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // LINKS
    QWidget *leftWidget = new QWidget(this);
    leftWidget->setStyleSheet("background: transparent; border: none;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(15);

    QLabel *title = new QLabel("Profil bearbeiten: " + m_profile.name, this);
    title->setStyleSheet("font-size: 18px; color: #5E5CE6; margin-bottom: 10px;");
    leftLayout->addWidget(title);

    auto addSlider = [this](QBoxLayout* l, QString text, int min, int max, int val, QSlider*& outSl, QLabel*& outLbl) {
        l->addWidget(new QLabel(text));
        QHBoxLayout *h = new QHBoxLayout;
        outSl = new QSlider(Qt::Horizontal);
        outSl->setRange(min, max);
        outSl->setValue(val);
        outLbl = new QLabel(QString::number(val));
        outLbl->setFixedWidth(50);
        outLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(outSl);
        h->addWidget(outLbl);
        l->addLayout(h);
        connect(outSl, &QSlider::valueChanged, this, &ProfileEditorDialog::onValuesChanged);
    };

    addSlider(leftLayout, "Element Größe (px)", 40, 500, m_profile.iconSize, m_slIconSize, m_lblIconVal);

    addSlider(leftLayout, "Button / Touch Target (px)", 24, 80, m_profile.buttonSize, m_slButtonSize, m_lblBtnVal);
    addSlider(leftLayout, "Toolbar Skalierung (%)", 50, 200, (int)(m_profile.toolbarScale * 100), m_slToolbarScale, m_lblToolbarVal);
    m_lblToolbarVal->setText(QString::number(m_slToolbarScale->value()) + "%");
    addSlider(leftLayout, "Raster Abstand (px)", 0, 100, m_profile.gridSpacing, m_slGridSpace, m_lblGridVal);

    m_chkSnap = new QCheckBox("Am Raster ausrichten", this);
    m_chkSnap->setChecked(m_profile.snapToGrid);
    connect(m_chkSnap, &QCheckBox::toggled, this, &ProfileEditorDialog::onValuesChanged);
    leftLayout->addWidget(m_chkSnap);

    leftLayout->addStretch();

    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *btnCancel = new QPushButton("Abbrechen", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    QPushButton *btnSave = new QPushButton("Speichern", this);
    btnSave->setStyleSheet("background-color: #5E5CE6; border: none; font-weight: bold;");
    connect(btnSave, &QPushButton::clicked, this, &QDialog::accept);
    btns->addWidget(btnCancel);
    btns->addWidget(btnSave);
    leftLayout->addLayout(btns);

    // RECHTS
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
