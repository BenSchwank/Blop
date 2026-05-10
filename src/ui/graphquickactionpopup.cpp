#include "graphquickactionpopup.h"
#include "tools/GraphCanvasItem.h"
#include "UIStyles.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
void applyPopupShadow(QFrame *card) {
    auto *shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(20);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 72));
    card->setGraphicsEffect(shadow);
}
} // namespace

GraphQuickActionPopup::GraphQuickActionPopup(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("GraphQuickActionPopup"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, false);
    setWindowFlags(Qt::Widget);

    const QString bg = UIStyles::Sidebar.name();
    const QString page = UIStyles::PageBackground.name();
    const QString text = UIStyles::Text.name();
    const QString sub = UIStyles::TextSecondary.name();

    setStyleSheet(QStringLiteral(
        "QWidget#GraphQuickActionPopup { background: transparent; }"
        "QFrame#GraphQuickCard { background-color: %1; border-radius: 14px; "
        "border: 1px solid rgba(124,92,252,0.42); }"
        "QFrame#GraphQuickCard QFrame#GraphQuickToolbar { background: transparent; border: none; }"
        "QFrame#GraphQuickCard QFrame#GraphQuickSectionRule { background: rgba(255,255,255,0.12); "
        "border: none; min-height: 1px; max-height: 1px; }"
        "QFrame#GraphQuickCard QWidget#GraphQuickX0Row { background-color: %2; "
        "border: 1px solid rgba(255,255,255,0.1); border-radius: 10px; }"
        "QFrame#GraphQuickCard QWidget#GraphQuickX0Row QLabel#GraphQuickX0Label { "
        "color: %4; font-size: 11px; font-weight: 700; letter-spacing: 0.2px; padding-left: 4px; }"
        "QFrame#GraphQuickCard QToolButton { background-color: rgba(255,255,255,0.14); "
        "border: 1px solid rgba(255,255,255,0.22); border-radius: 8px; min-width: 34px; min-height: 34px; "
        "color: %3; font-weight: 700; font-size: 13px; }"
        "QFrame#GraphQuickCard QToolButton:hover { background-color: rgba(124,92,252,0.35); }"
        "QFrame#GraphQuickCard QDoubleSpinBox { background-color: %2; color: %3; "
        "border: 1px solid rgba(255,255,255,0.15); border-radius: 8px; padding: 3px 6px; min-height: 28px; }")
                      .arg(bg, page, text, sub));

    auto *outerLay = new QVBoxLayout(this);
    outerLay->setContentsMargins(5, 5, 5, 5);
    outerLay->setSpacing(0);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("GraphQuickCard"));
    card->setAttribute(Qt::WA_StyledBackground, true);
    applyPopupShadow(card);

    auto *v = new QVBoxLayout(card);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(8);

    auto *toolbar = new QFrame(card);
    toolbar->setObjectName(QStringLiteral("GraphQuickToolbar"));
    auto *row = new QHBoxLayout(toolbar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_btnAxes = new QToolButton(toolbar);
    m_btnAxes->setText(QStringLiteral("f"));
    m_btnAxes->setToolTip(QStringLiteral("Achsen und Teilstriche"));
    connect(m_btnAxes, &QToolButton::clicked, this, [this]() { emit axisSettingsRequested(); });

    m_btnAnalyse = new QToolButton(toolbar);
    m_btnAnalyse->setText(QStringLiteral("A"));
    m_btnAnalyse->setToolTip(QStringLiteral("Analyse (Ableitung, Nullstellen, Extrema)"));
    m_menuAnalyse = new QMenu(this);
    rebuildAnalyseMenu();
    m_btnAnalyse->setMenu(m_menuAnalyse);
    m_btnAnalyse->setPopupMode(QToolButton::InstantPopup);

    m_btnTangent = new QToolButton(toolbar);
    m_btnTangent->setText(QStringLiteral("T"));
    m_btnTangent->setToolTip(QStringLiteral("Stelle / Tangente (Popup)"));
    connect(m_btnTangent, &QToolButton::clicked, this, [this]() { emit tangentManualRequested(); });

    m_btnDelFn = new QToolButton(toolbar);
    m_btnDelFn->setText(QStringLiteral("−"));
    m_btnDelFn->setToolTip(QStringLiteral("Ausgewaehlte Funktion entfernen"));
    connect(m_btnDelFn, &QToolButton::clicked, this, [this]() { emit removeSelectedFunctionRequested(); });

    m_btnDelGraph = new QToolButton(toolbar);
    m_btnDelGraph->setText(QStringLiteral("X"));
    m_btnDelGraph->setToolTip(QStringLiteral("Gesamte Grafik loeschen"));
    connect(m_btnDelGraph, &QToolButton::clicked, this, [this]() { emit removeGraphRequested(); });

    row->addWidget(m_btnAxes);
    row->addWidget(m_btnAnalyse);
    row->addWidget(m_btnTangent);
    row->addWidget(m_btnDelFn);
    row->addWidget(m_btnDelGraph);
    v->addWidget(toolbar);

    auto *rule = new QFrame(card);
    rule->setObjectName(QStringLiteral("GraphQuickSectionRule"));
    rule->setFrameShape(QFrame::NoFrame);
    v->addWidget(rule);

    auto *x0Row = new QWidget(card);
    x0Row->setObjectName(QStringLiteral("GraphQuickX0Row"));
    x0Row->setAttribute(Qt::WA_StyledBackground, true);
    auto *x0Lay = new QHBoxLayout(x0Row);
    x0Lay->setContentsMargins(8, 6, 8, 6);
    x0Lay->setSpacing(8);

    auto *xLbl = new QLabel(QStringLiteral("x0"), x0Row);
    xLbl->setObjectName(QStringLiteral("GraphQuickX0Label"));
    m_x0 = new QDoubleSpinBox(x0Row);
    m_x0->setRange(-100000.0, 100000.0);
    m_x0->setDecimals(3);
    m_x0->setSingleStep(0.25);
    m_x0->setToolTip(QStringLiteral("Tangente: Beruehrpunkt"));
    connect(m_x0, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { emit tangentXRequested(v); });
    m_btnTangentAtRoot = new QToolButton(x0Row);
    m_btnTangentAtRoot->setText(QStringLiteral("N"));
    m_btnTangentAtRoot->setToolTip(QStringLiteral("x0 auf erste Nullstelle"));
    connect(m_btnTangentAtRoot, &QToolButton::clicked, this,
            [this]() { emit tangentAtFirstRootRequested(); });
    x0Lay->addWidget(xLbl, 0, Qt::AlignVCenter);
    x0Lay->addWidget(m_x0, 1);
    x0Lay->addWidget(m_btnTangentAtRoot, 0, Qt::AlignVCenter);
    v->addWidget(x0Row);

    outerLay->addWidget(card);

    hide();
}

void GraphQuickActionPopup::rebuildAnalyseMenu() {
    if (!m_menuAnalyse)
        return;
    m_menuAnalyse->clear();
    const QString page = UIStyles::PageBackground.name();
    const QString text = UIStyles::Text.name();
    m_menuAnalyse->setStyleSheet(QStringLiteral(
        "QMenu { background-color: %1; color: %2; border: 1px solid rgba(124,92,252,0.4); "
        "border-radius: 8px; padding: 4px; }"
        "QMenu::item { padding: 8px 24px 8px 12px; border-radius: 6px; }"
        "QMenu::item:selected { background-color: rgba(124,92,252,0.35); }")
                                     .arg(page, text));
    connect(m_menuAnalyse->addAction(QStringLiteral("Ableitungskurve")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("derivative_create")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Nullstellen")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("roots")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Extremstellen")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("extrema")); });
}

void GraphQuickActionPopup::bind(GraphCanvasItem *item) {
    if (!m_x0)
        return;
    m_x0->blockSignals(true);
    if (!item) {
        m_x0->setValue(0.0);
        m_x0->blockSignals(false);
        m_x0->setEnabled(false);
        m_btnDelFn->setEnabled(false);
        m_btnTangentAtRoot->setEnabled(false);
        return;
    }
    const GraphObject d = item->data();
    m_x0->setRange(d.xMin, d.xMax);
    const bool hasFn = d.selectedFunction >= 0 && d.selectedFunction < d.functions.size();
    m_x0->setEnabled(hasFn);
    m_btnDelFn->setEnabled(hasFn);
    m_btnTangentAtRoot->setEnabled(hasFn);
    if (hasFn)
        m_x0->setValue(d.functions[d.selectedFunction].tangentX);
    else
        m_x0->setValue(0.0);
    m_x0->blockSignals(false);
    adjustSize();
}
