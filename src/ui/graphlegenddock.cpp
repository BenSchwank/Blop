#include "graphlegenddock.h"
#include "tools/GraphCanvasItem.h"
#include "UIStyles.h"

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
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 72));
    card->setGraphicsEffect(shadow);
}
} // namespace

GraphLegendDock::GraphLegendDock(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("GraphLegendDock"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, false);

    const QString bg = UIStyles::Sidebar.name();
    const QString page = UIStyles::PageBackground.name();
    const QString accent = UIStyles::Accent.name();
    const QString text = UIStyles::Text.name();
    const QString sub = UIStyles::TextSecondary.name();

    setStyleSheet(QStringLiteral(
        "QWidget#GraphLegendDock { background: transparent; }"
        "QFrame#GraphLegendCard { background-color: %1; border-radius: 14px; "
        "border: 1px solid rgba(124,92,252,0.38); color: %3; }"
        "QFrame#GraphLegendCard QLabel { color: %3; background: transparent; border: none; }"
        "QFrame#GraphLegendCard QLabel#GraphLegendSection { color: %4; font-size: 11px; font-weight: 700; "
        "letter-spacing: 0.3px; }"
        "QFrame#GraphLegendCard QLabel#GraphLegendActive { color: %4; font-size: 10px; font-weight: 600; "
        "padding: 0 0 4px 0; background: transparent; border: none; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendListInset { background-color: %5; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 10px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow { background: transparent; border: none; "
        "border-bottom: 1px solid rgba(255,255,255,0.08); border-radius: 0; min-height: 36px; max-height: 36px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow:hover[selected=\"false\"] { "
        "background-color: rgba(255,255,255,0.06); }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow QToolButton#GraphLegendRowSelect { "
        "background: transparent; border: none; color: %3; font-size: 12px; font-weight: 500; "
        "text-align: left; padding: 4px 8px; min-height: 32px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow QToolButton#GraphLegendRowSelect:checked { "
        "font-weight: 700; color: #f0ecff; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow QToolButton#GraphLegendRowRemove { "
        "background: transparent; border: none; color: %4; font-size: 14px; font-weight: 600; "
        "min-width: 28px; max-width: 28px; min-height: 28px; border-radius: 6px; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendRow QToolButton#GraphLegendRowRemove:hover { "
        "background-color: rgba(220,80,90,0.35); color: #ffd0d4; }"
        "QFrame#GraphLegendCard QWidget#GraphLegendFooter { background: transparent; border: none; }"
        "QFrame#GraphLegendCard QFrame#GraphLegendFooterRule { background: rgba(255,255,255,0.12); "
        "border: none; min-height: 1px; max-height: 1px; }"
        "QFrame#GraphLegendCard QPushButton { background-color: rgba(255,255,255,0.12); "
        "border: 1px solid rgba(255,255,255,0.2); border-radius: 8px; padding: 7px 10px; "
        "min-height: 32px; color: %3; font-size: 12px; font-weight: 600; }"
        "QFrame#GraphLegendCard QPushButton:hover { background-color: rgba(124,92,252,0.28); "
        "border-color: rgba(124,92,252,0.5); }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendPrimary { background-color: %2; color: #f6f4ff; border: none; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendPrimary:hover { background-color: #8b6eff; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendDanger { background-color: rgba(220,80,90,0.2); "
        "border-color: rgba(220,80,90,0.45); color: #ffb8bc; }"
        "QFrame#GraphLegendCard QPushButton#GraphLegendDanger:hover { background-color: rgba(220,80,90,0.32); }")
                      .arg(bg, accent, text, sub, page));

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
    cardLay->addWidget(m_lblActiveFn);

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
    m_btnAddFunction->setToolTip(QStringLiteral("Handschrift oder Texteingabe"));
    m_btnAddFunction->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    footerLay->addWidget(m_btnAddFunction);

    m_btnRemoveGraph = new QPushButton(QStringLiteral("Grafik loeschen"), m_footer);
    m_btnRemoveGraph->setObjectName(QStringLiteral("GraphLegendDanger"));
    m_btnRemoveGraph->setToolTip(QStringLiteral("Koordinatensystem entfernen"));
    m_btnRemoveGraph->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    footerLay->addWidget(m_btnRemoveGraph);

    cardLay->addWidget(m_footer);

    outerLay->addWidget(m_card);

    setFixedWidth(228);

    connect(m_btnAddFunction, &QPushButton::clicked, this, [this]() { emit entryBarRequested(); });
    connect(m_btnRemoveGraph, &QPushButton::clicked, this, [this]() { emit removeGraphWidgetRequested(); });
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
    row->setAttribute(Qt::WA_StyledBackground, true);
    row->setProperty("selected", selected);

    const QColor chipColor = curveColor.isValid() ? curveColor : UIStyles::Accent;
    const QString chipCss = QStringLiteral(
                               "QFrame#GraphLegendColorChip { background-color: %1; border: 1px solid rgba(255,255,255,0.22); "
                               "border-radius: 4px; min-width: 5px; max-width: 5px; min-height: 24px; max-height: 24px; }")
                                .arg(chipColor.name(QColor::HexRgb));
    const QString rowBase = QStringLiteral(
        "QWidget#GraphLegendRow { border: none; border-bottom: 1px solid rgba(255,255,255,0.08); "
        "min-height: 36px; max-height: 36px; ");
    if (selected) {
        row->setStyleSheet(rowBase + QStringLiteral("background-color: rgba(124,92,252,0.16); border-left: 4px solid %1; }")
                               .arg(chipColor.name(QColor::HexRgb)));
    } else {
        row->setStyleSheet(rowBase + QStringLiteral("background: transparent; border-left: 4px solid rgba(255,255,255,0.08); }"));
    }

    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(6, 0, 4, 0);
    rowLay->setSpacing(6);

    auto *chip = new QFrame(row);
    chip->setObjectName(QStringLiteral("GraphLegendColorChip"));
    chip->setFixedSize(5, 24);
    chip->setStyleSheet(chipCss);

    QString label = expression;
    if (label.size() > 20)
        label = label.left(18) + QStringLiteral("…");
    auto *selBtn = new QToolButton(row);
    selBtn->setObjectName(QStringLiteral("GraphLegendRowSelect"));
    selBtn->setCheckable(true);
    selBtn->setChecked(false);
    selBtn->setText(label);
    selBtn->setToolTip(expression);
    selBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    selBtn->setToolButtonStyle(Qt::ToolButtonTextOnly);

    auto *closeBtn = new QToolButton(row);
    closeBtn->setObjectName(QStringLiteral("GraphLegendRowRemove"));
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setToolTip(QStringLiteral("Funktion entfernen"));

    m_fnGroup->addButton(selBtn, index);
    connect(closeBtn, &QToolButton::clicked, this, [this, index]() { emit removeRequested(index); });

    rowLay->addWidget(chip, 0, Qt::AlignVCenter);
    rowLay->addWidget(selBtn, 1);
    rowLay->addWidget(closeBtn, 0, Qt::AlignVCenter);

    m_rowLayout->addWidget(row);
    if (QStyle *st = row->style()) {
        st->unpolish(row);
        st->polish(row);
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

    if (d.selectedFunction >= 0 && d.selectedFunction < d.functions.size()) {
        QString expr = d.functions[d.selectedFunction].expression;
        if (expr.size() > 45)
            expr = expr.left(42) + QStringLiteral("…");
        m_lblActiveFn->setText(QStringLiteral("Aktiv: %1").arg(expr));
        m_lblActiveFn->show();
    }

    if (d.functions.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("Noch keine Funktion"), m_listInset);
        empty->setStyleSheet(QStringLiteral("QLabel { color: %1; font-style: italic; font-size: 11px; padding: 8px 10px; }")
                                 .arg(UIStyles::TextSecondary.name()));
        m_rowLayout->addWidget(empty);
    }
    setFixedWidth(d.functions.isEmpty() ? 200 : 228);
    adjustSize();
}
