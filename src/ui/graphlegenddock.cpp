#include "graphlegenddock.h"
#include "tools/GraphCanvasItem.h"
#include "notechrome.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QColor>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
void applyListShadow(QFrame *card) {
#ifdef Q_OS_ANDROID
    // v3.17.4: Android software path makes the offscreen pixmap from
    // QGraphicsDropShadowEffect a permanent per-frame cost while the dock
    // is visible. The dock has a 1px border via its stylesheet already;
    // skipping the shadow buys back ~3-4 ms/frame on mid-range devices.
    Q_UNUSED(card);
#else
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 72));
    card->setGraphicsEffect(shadow);
#endif
}
} // namespace

GraphLegendDock::GraphLegendDock(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("GraphLegendDock"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, false);
    applyCardStyle();

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(5, 5, 5, 5);
    outerLay->setSpacing(0);

    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("GraphLegendCard"));
    m_card->setAttribute(Qt::WA_StyledBackground, true);
    applyListShadow(m_card);

    auto *cardLay = new QVBoxLayout(m_card);
    cardLay->setContentsMargins(12, 12, 12, 12);
    cardLay->setSpacing(10);

    auto *fnLbl = new QLabel(QStringLiteral("Funktionen"), m_card);
    fnLbl->setObjectName(QStringLiteral("GraphLegendSection"));
    cardLay->addWidget(fnLbl);

    m_lblActiveFn = new QLabel(m_card);
    m_lblActiveFn->setObjectName(QStringLiteral("GraphLegendActive"));
    m_lblActiveFn->setWordWrap(true);
    m_lblActiveFn->hide();

    m_fnGroup = new QButtonGroup(this);
    m_fnGroup->setExclusive(true);
    connect(m_fnGroup, &QButtonGroup::idClicked, this, [this](int id) {
        if (id >= 0)
            emit selectionRequested(id);
    });

    m_listInset = new QWidget(m_card);
    m_listInset->setObjectName(QStringLiteral("GraphLegendListInset"));
    m_listInset->setAttribute(Qt::WA_StyledBackground, true);
    m_listInset->setMinimumHeight(40);
    m_rowLayout = new QVBoxLayout(m_listInset);
    m_rowLayout->setContentsMargins(0, 4, 0, 4);
    m_rowLayout->setSpacing(0);
    cardLay->addWidget(m_listInset);

    m_footer = new QWidget(m_card);
    m_footer->setObjectName(QStringLiteral("GraphLegendFooter"));
    auto *footerLay = new QVBoxLayout(m_footer);
    footerLay->setContentsMargins(0, 2, 0, 0);
    footerLay->setSpacing(10);
    auto *rule = new QFrame(m_footer);
    rule->setObjectName(QStringLiteral("GraphLegendFooterRule"));
    rule->setFrameShape(QFrame::NoFrame);
    footerLay->addWidget(rule);

    m_btnAddFunction = new QPushButton(QStringLiteral("+ Funktion"), m_footer);
    m_btnAddFunction->setObjectName(QStringLiteral("GraphLegendPrimary"));
    m_btnAddFunction->setToolTip(QStringLiteral("Auf die Linie schreiben"));
    m_btnAddFunction->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    footerLay->addWidget(m_btnAddFunction);

    m_btnRemoveGraph = new QPushButton(QStringLiteral("Graph entfernen"), m_footer);
    m_btnRemoveGraph->setObjectName(QStringLiteral("GraphLegendDanger"));
    m_btnRemoveGraph->setToolTip(QStringLiteral("Koordinatensystem entfernen"));
    m_btnRemoveGraph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    footerLay->addWidget(m_btnRemoveGraph);

    cardLay->addWidget(m_footer);

    outerLay->addWidget(m_card);

    setFixedWidth(220);

    connect(m_btnAddFunction, &QPushButton::clicked, this, [this]() { emit entryBarRequested(); });
    connect(m_btnRemoveGraph, &QPushButton::clicked, this, [this]() { emit removeGraphWidgetRequested(); });
}

void GraphLegendDock::refreshChrome() {
    applyCardStyle();
}

void GraphLegendDock::applyCardStyle() {
    const QString bg = NoteChrome::panelElevated().name(QColor::HexRgb);
    const QString page = NoteChrome::panelBg().name(QColor::HexRgb);
    const QString accent = NoteChrome::accent().name(QColor::HexRgb);
    const QString text = NoteChrome::textPrimary().name(QColor::HexRgb);
    const QString sub = NoteChrome::textSecondary().name(QColor::HexRgb);
    const QString accentHover = NoteChrome::rgbaCss(NoteChrome::accent(), 72);
    const QString accentBorder = NoteChrome::rgbaCss(NoteChrome::accent(), 128);
    setStyleSheet(QStringLiteral(
        "QWidget#GraphLegendDock { background: transparent; }"
        "QFrame#GraphLegendCard { background-color: %1; border-radius: 16px; "
        "border: 1px solid %6; color: %3; }"
        "QFrame#GraphLegendCard QLabel { color: %3; background: transparent; border: none; }"
        "QFrame#GraphLegendCard QLabel#GraphLegendSection { color: %4; font-size: 10px; font-weight: 700; "
        "letter-spacing: 0.6px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendListInset { background: transparent; "
        "border: none; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow { background: transparent; border: none; "
        "min-height: 32px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendChip { background-color: %5; "
        "border: 1px solid %7; border-radius: 16px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendChip[selected=\"true\"] { "
        "background-color: %8; border: 1px solid %6; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendChip QToolButton#GraphLegendRowSelect { "
        "background: transparent; border: none; color: %3; font-size: 12px; font-weight: 600; "
        "text-align: left; padding: 2px 4px; min-height: 24px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendChip QToolButton#GraphLegendRowRemove { "
        "background: transparent; border: none; color: %4; font-size: 14px; font-weight: 600; "
        "min-width: 22px; max-width: 22px; min-height: 22px; border-radius: 11px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendChip QToolButton#GraphLegendRowRemove:hover { "
        "background-color: rgba(220,80,90,0.35); color: #ffd0d4; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendFooter { background: transparent; border: none; }"
        "QFrame#GraphLegendCard QFrame#GraphLegendFooterRule { background: %7; "
        "border: none; min-height: 1px; max-height: 1px; }"
        "QFrame#GraphLegendCard QPushButton { background-color: rgba(255,255,255,0.08); "
        "border: 1px solid %7; border-radius: 14px; padding: 6px 10px; "
        "min-height: 28px; color: %3; font-size: 11px; font-weight: 600; }"
        "QFrame#GraphLegendCard QPushButton:hover { background-color: %8; "
        "border-color: %6; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendPrimary { background-color: %2; color: #0f172a; border: none; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendPrimary:hover { background-color: %9; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendDanger { background-color: transparent; "
        "border: none; color: %4; min-height: 22px; padding: 2px 6px; font-weight: 500; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendDanger:hover { color: #ffb8bc; }")
                      .arg(bg, accent, text, sub, page, accentBorder,
                           NoteChrome::rgbaCss(NoteChrome::border(), 80),
                           accentHover,
                           NoteChrome::accent().lighter(110).name(QColor::HexRgb)));
}

void GraphLegendDock::clearRowLayouts() {
    if (m_fnGroup) {
        const QList<QAbstractButton *> oldBtns = m_fnGroup->buttons();
        for (QAbstractButton *b : oldBtns)
            m_fnGroup->removeButton(b);
    }
    QLayoutItem *child = nullptr;
    while ((child = m_rowLayout->takeAt(0)) != nullptr) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }
}

void GraphLegendDock::addFunctionRow(int index, const QString &expression, bool selected, const QColor &curveColor) {
    auto *row = new QWidget(m_listInset);
    row->setObjectName(QStringLiteral("GraphLegendRow"));

    auto *chip = new QWidget(row);
    chip->setObjectName(QStringLiteral("GraphLegendChip"));
    chip->setAttribute(Qt::WA_StyledBackground, true);
    chip->setProperty("selected", selected);

    const QColor chipColor = curveColor.isValid() ? curveColor : NoteChrome::accent();
    auto *dot = new QFrame(chip);
    dot->setFixedSize(8, 8);
    dot->setStyleSheet(QStringLiteral(
                           "QFrame { background-color: %1; border-radius: 4px; border: none; }")
                           .arg(chipColor.name(QColor::HexRgb)));

    QString label = expression;
    if (label.size() > 22)
        label = label.left(20) + QStringLiteral("…");
    auto *selBtn = new QToolButton(chip);
    selBtn->setObjectName(QStringLiteral("GraphLegendRowSelect"));
    selBtn->setCheckable(true);
    selBtn->setChecked(selected);
    selBtn->setText(label);
    selBtn->setToolTip(QStringLiteral("Tippen: auswählen · Nullstellen ziehen"));
    selBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto *closeBtn = new QToolButton(chip);
    closeBtn->setObjectName(QStringLiteral("GraphLegendRowRemove"));
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setToolTip(QStringLiteral("Funktion entfernen"));

    m_fnGroup->addButton(selBtn, index);
    connect(closeBtn, &QToolButton::clicked, this, [this, index]() { emit removeRequested(index); });

    auto *chipLay = new QHBoxLayout(chip);
    chipLay->setContentsMargins(10, 4, 4, 4);
    chipLay->setSpacing(6);
    chipLay->addWidget(dot, 0, Qt::AlignVCenter);
    chipLay->addWidget(selBtn, 1);
    chipLay->addWidget(closeBtn, 0, Qt::AlignVCenter);

    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->addWidget(chip);

    m_rowLayout->addWidget(row);
    if (QStyle *st = chip->style()) {
        st->unpolish(chip);
        st->polish(chip);
    }
}

void GraphLegendDock::bind(GraphCanvasItem *item) {
    clearRowLayouts();
    m_selectedIdx = -1;
    if (m_lblActiveFn)
        m_lblActiveFn->hide();
    if (!item) {
        m_btnRemoveGraph->hide();
        m_footer->setVisible(false);
        return;
    }
    m_btnRemoveGraph->show();
    m_footer->setVisible(true);
    const auto &d = item->data();
    m_selectedIdx = d.selectedFunction;

    {
        const QSignalBlocker blocker(m_fnGroup);
        for (int i = 0; i < d.functions.size(); ++i)
            addFunctionRow(i, d.functions[i].expression, i == d.selectedFunction, d.functions[i].color);
        if (m_selectedIdx >= 0 && m_selectedIdx < d.functions.size()) {
            if (auto *b = m_fnGroup->button(m_selectedIdx))
                b->setChecked(true);
        } else if (!d.functions.isEmpty()) {
            if (auto *b = m_fnGroup->button(0))
                b->setChecked(true);
        }
    }

    if (d.functions.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("Auf die Linie schreiben"), m_listInset);
        empty->setStyleSheet(QStringLiteral("QLabel { color: %1; font-style: italic; font-size: 11px; padding: 6px 4px; }")
                                 .arg(NoteChrome::textSecondary().name(QColor::HexRgb)));
        m_rowLayout->addWidget(empty);
    }
    setFixedWidth(220);
    adjustSize();
}
