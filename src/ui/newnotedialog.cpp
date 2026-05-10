#include "newnotedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>

NewNoteDialog::NewNoteDialog(QWidget *parent) : QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    resize(400, 340);
    setupUi();
}

void NewNoteDialog::setupUi()
{
    // Haupt-Container mit Styling
    QWidget *container = new QWidget(this);
    container->setGeometry(5, 5, width()-10, height()-10); // Platz für Schatten
    container->setStyleSheet(
        "QWidget { background-color: #1E1E1E; border: 1px solid #444; border-radius: 12px; }"
        "QLabel { color: #DDD; font-family: 'Segoe UI'; border: none; background: transparent; }"
        "QLineEdit { background: #252526; color: white; border: 1px solid #444; border-radius: 6px; padding: 8px; font-size: 14px; selection-background-color: #5E5CE6; }"
        "QLineEdit:focus { border: 1px solid #5E5CE6; }"
        );

    // Schatten
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(15);
    shadow->setColor(QColor(0,0,0,150));
    shadow->setOffset(0, 4);
    container->setGraphicsEffect(shadow);

    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(20);

    // Titel
    QLabel *lblTitle = new QLabel("Neue Notiz erstellen", container);
    lblTitle->setStyleSheet("font-size: 18px; font-weight: bold; color: white;");
    layout->addWidget(lblTitle);

    // Name Input
    QVBoxLayout *nameLay = new QVBoxLayout;
    nameLay->setSpacing(8);
    QLabel *lblName = new QLabel("Name", container);
    lblName->setStyleSheet("font-size: 13px; color: #BBB; font-weight: bold;");
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
    lblFormat->setStyleSheet("font-size: 13px; color: #BBB; font-weight: bold;");

    QHBoxLayout *btnsLay = new QHBoxLayout;
    btnsLay->setSpacing(10);

    // Helper für Format Buttons
    auto createFormatBtn = [this](QString text, QString subtext) -> QPushButton* {
        QPushButton *btn = new QPushButton(this);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(70);
        btn->setText(text + "\n" + subtext);
        btn->setStyleSheet(
            "QPushButton { background: #252526; color: #AAA; border: 1px solid #444; border-radius: 8px; text-align: left; padding: 10px; line-height: 1.2; font-size: 14px; }"
            "QPushButton:checked { background: #5E5CE6; color: white; border: 1px solid #5E5CE6; }"
            "QPushButton:hover:!checked { background: #333; border-color: #555; }"
            );
        return btn;
    };

    m_btnFormatInfinite = createFormatBtn("Unendlich", "Freie Leinwand\n(Standard)");
    m_btnFormatA4 = createFormatBtn("DIN A4", "Seitenbasiert\n(Druckoptimiert)");
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
    m_btnCancel->setStyleSheet("QPushButton { background: transparent; color: #AAA; border: none; font-weight: bold; font-size: 14px; } QPushButton:hover { color: white; }");
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    m_btnCreate = new QPushButton("Erstellen", container);
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    m_btnCreate->setFixedSize(100, 36);
    m_btnCreate->setStyleSheet("QPushButton { background: #5E5CE6; color: white; border: none; border-radius: 18px; font-weight: bold; font-size: 14px; } QPushButton:hover { background: #4b49c9; }");
    connect(m_btnCreate, &QPushButton::clicked, this, &QDialog::accept);

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
