#include "newnotedialog.h"
#include "blop_theme.h"
#include "blopripple.h"
#include "notepreviewicon.h"
#include <QIcon>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QShowEvent>

NewNoteDialog::NewNoteDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground, false);
    resize(420, 360);
    setupUi();
}

void NewNoteDialog::setupUi()
{
    QWidget *container = new QWidget(this);
    container->setObjectName(QStringLiteral("NewNoteCard"));
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(container);
    container->setStyleSheet(
        QStringLiteral("QWidget#NewNoteCard { background-color: %1; border: none; }")
            .arg(BlopTheme::surfaceElevated().name(QColor::HexRgb)) +
        BlopTheme::themed(QStringLiteral(
            "QLabel { color: #DDD; font-family: 'Segoe UI'; border: none; background: transparent; }"
            "QLineEdit { background: rgba(22, 24, 36, 0.95); color: #F4F5FB; border: 1px solid rgba(120,130,160,0.32); border-radius: 8px; padding: 8px; font-size: 14px; selection-background-color: #7C5CFC; }"
            "QLineEdit:focus { border: 1px solid #7C5CFC; }"))
        );

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(20);

    // Titel
    QLabel *lblTitle = new QLabel("Neue Notiz erstellen", container);
    lblTitle->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "font-size: 18px; font-weight: bold; color: #F4F5FB;")));
    layout->addWidget(lblTitle);

    // Name Input
    QVBoxLayout *nameLay = new QVBoxLayout;
    nameLay->setSpacing(8);
    QLabel *lblName = new QLabel("Name", container);
    lblName->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "font-size: 13px; color: #BBB; font-weight: bold;")));
    m_nameInput = new QLineEdit(container);
    m_nameInput->setPlaceholderText("Meine Notiz");
    m_nameInput->setFocus();
    nameLay->addWidget(lblName);
    nameLay->addWidget(m_nameInput);
    layout->addLayout(nameLay);

    // Format Auswahl
    QVBoxLayout *formatLay = new QVBoxLayout;
    formatLay->setSpacing(8);
    QLabel *lblFormat = new QLabel("Format (nicht änderbar)", container);
    lblFormat->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "font-size: 13px; color: #BBB; font-weight: bold;")));

    QHBoxLayout *btnsLay = new QHBoxLayout;
    btnsLay->setSpacing(10);

    // Helper für Format Buttons
    auto createFormatBtn = [this](QString text, QString subtext) -> QPushButton* {
        QPushButton *btn = new QPushButton(this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(78);
        btn->setIconSize(QSize(40, 40));
        btn->setText(text + "\n" + subtext);
        btn->setStyleSheet(BlopTheme::themed(QStringLiteral(
            "QPushButton { background: #252526; color: #AAA; border: 1px solid #444; border-radius: 8px; text-align: left; padding: 10px; line-height: 1.2; font-size: 14px; }"
            "QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }"
            "QPushButton:hover:!checked { background: #333; border-color: #555; }"
            )));
        BlopRipple::attachPressFeedback(btn, 0.92);
        return btn;
    };

    m_btnFormatInfinite = createFormatBtn("Unendlich", "Freie Leinwand\n(Standard)");
    m_btnFormatA4 = createFormatBtn("DIN A4", "Seitenbasiert\n(Druckoptimiert)");
    {
      NotePreviewIcon::Spec inf;
      inf.kind = NotePreviewIcon::Kind::Infinite;
      inf.backgroundType = 2;
      m_btnFormatInfinite->setIcon(QIcon(NotePreviewIcon::pixmap(inf, 48)));
      NotePreviewIcon::Spec a4;
      a4.kind = NotePreviewIcon::Kind::A4;
      a4.backgroundType = 1;
      m_btnFormatA4->setIcon(QIcon(NotePreviewIcon::pixmap(a4, 48)));
    }
    m_btnFormatInfinite->setChecked(true); // Default

    m_groupFormat = new QButtonGroup(this);
    m_groupFormat->addButton(m_btnFormatInfinite, 0);
    m_groupFormat->addButton(m_btnFormatA4, 1);
    m_groupFormat->setExclusive(true);

    btnsLay->addWidget(m_btnFormatInfinite);
    btnsLay->addWidget(m_btnFormatA4);

    formatLay->addWidget(lblFormat);
    formatLay->addLayout(btnsLay);
    layout->addLayout(formatLay);

    layout->addStretch();

    // Action Buttons
    QHBoxLayout *actionLay = new QHBoxLayout;
    actionLay->addStretch();

    m_btnCancel = new QPushButton("Abbrechen", container);
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    m_btnCancel->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: transparent; color: #AAA; border: none; font-weight: bold; font-size: 14px; } QPushButton:hover { color: #F4F5FB; }")));
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnCreate = new QPushButton("Erstellen", container);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setFixedSize(100, 36);
    m_btnCreate->setStyleSheet(BlopTheme::themed(QStringLiteral(
        "QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 18px; font-weight: bold; font-size: 14px; } QPushButton:hover { background: #4b49c9; }")));
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);
    BlopRipple::attachPressFeedback(m_btnCancel, 0.92);
    BlopRipple::attachPressFeedback(m_btnCreate, 0.92);

    actionLay->addWidget(m_btnCancel);
    actionLay->addWidget(m_btnCreate);
    layout->addLayout(actionLay);
}

QString NewNoteDialog::getNoteName() const {
    QString t = m_nameInput->text().trimmed();
    return t.isEmpty() ? "Neue Notiz" : t;
}

bool NewNoteDialog::isInfiniteFormat() const {
    return m_btnFormatInfinite->isChecked();
}

void NewNoteDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    setWindowOpacity(1.0);
    if (!isWindow())
        return;
    if (!parentWidget())
        return;
    if (m_dialogIntroDone)
        return;
    m_dialogIntroDone = true;
    // v3.16.1: unified slide-in curve (280ms OutCubic + 220ms opacity) so all
    // dialogs read as one design language.
    const QPoint dest = pos();
#ifndef Q_OS_ANDROID
    setWindowOpacity(0.0);
    auto *opAnim = new QPropertyAnimation(this, "windowOpacity", this);
    opAnim->setDuration(BlopMotion::kStandard);
    opAnim->setStartValue(0.0);
    opAnim->setEndValue(1.0);
    opAnim->setEasingCurve(BlopMotion::kEaseStandard);
    opAnim->start(QAbstractAnimation::DeleteWhenStopped);
#endif
    move(dest.x(), dest.y() + 24);
    auto *posAnim = new QPropertyAnimation(this, "pos", this);
    posAnim->setDuration(BlopMotion::kEmphasis);
    posAnim->setStartValue(QPoint(dest.x(), dest.y() + 24));
    posAnim->setEndValue(dest);
    posAnim->setEasingCurve(BlopMotion::kEaseStandard);
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NewNoteDialog::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void NewNoteDialog::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}
