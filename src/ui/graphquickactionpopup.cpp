#include "graphquickactionpopup.h"
#include "tools/GraphCanvasItem.h"
#include "tools/math/NumericAnalysis.h"
#include "notechrome.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QSlider>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
void applyPopupShadow(QFrame *card) {
#ifdef Q_OS_ANDROID
    // v3.17.4: skip drop-shadow on Android. The popup stays visible while
    // the user interacts with the graph; its offscreen rasterisation pass
    // costs ~3 ms/frame on the software path.
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

GraphQuickActionPopup::GraphQuickActionPopup(QWidget *parent) : QWidget(parent) {
    setObjectName(QStringLiteral("GraphQuickActionPopup"));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground, false);
    setWindowFlags(Qt::Widget);
    applyCardStyle();

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
    m_btnAnalyse->setText(QStringLiteral("K"));
    m_btnAnalyse->setToolTip(QStringLiteral("Kurvendiskussion"));
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

    m_paramRow = new QWidget(card);
    m_paramRow->setObjectName(QStringLiteral("GraphQuickParamRow"));
    auto *pLay = new QHBoxLayout(m_paramRow);
    pLay->setContentsMargins(8, 6, 8, 6);
    pLay->setSpacing(8);
    m_paramLabel = new QLabel(QStringLiteral("a"), m_paramRow);
    m_paramSlider = new QSlider(Qt::Horizontal, m_paramRow);
    m_paramSlider->setRange(0, 1000);
    m_paramValue = new QLabel(QStringLiteral("1"), m_paramRow);
    m_paramValue->setMinimumWidth(36);
    m_paramValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pLay->addWidget(m_paramLabel, 0, Qt::AlignVCenter);
    pLay->addWidget(m_paramSlider, 1);
    pLay->addWidget(m_paramValue, 0, Qt::AlignVCenter);
    connect(m_paramSlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_syncingParam || !m_paramSlider)
            return;
        const double t = v / 1000.0;
        // Map via stored min/max from last bind (encoded in slider property).
        const double pmin = m_paramSlider->property("pmin").toDouble();
        const double pmax = m_paramSlider->property("pmax").toDouble();
        const double val = pmin + t * (pmax - pmin);
        if (m_paramValue)
            m_paramValue->setText(QString::number(val, 'g', 3));
        emit paramValueRequested(val);
    });
    v->addWidget(m_paramRow);
    m_paramRow->hide();

    outerLay->addWidget(card);

    hide();
}

void GraphQuickActionPopup::refreshChrome() {
    applyCardStyle();
    rebuildAnalyseMenu();
}

void GraphQuickActionPopup::applyCardStyle() {
    const QString bg = NoteChrome::panelElevated().name(QColor::HexRgb);
    const QString page = NoteChrome::panelBg().name(QColor::HexRgb);
    const QString text = NoteChrome::textPrimary().name(QColor::HexRgb);
    const QString sub = NoteChrome::textSecondary().name(QColor::HexRgb);
    setStyleSheet(QStringLiteral(
        "QWidget#GraphQuickActionPopup { background: transparent; }"
        "QFrame#GraphQuickCard { background-color: %1; border-radius: 14px; "
        "border: 1px solid %5; }"
        "QFrame#GraphQuickCard QFrame#GraphQuickToolbar { background: transparent; border: none; }"
        "QFrame#GraphQuickCard QFrame#GraphQuickSectionRule { background: %6; "
        "border: none; min-height: 1px; max-height: 1px; }"
        "QFrame#GraphQuickCard QWidget#GraphQuickX0Row { background-color: %2; "
        "border: 1px solid %6; border-radius: 10px; }"
        "QFrame#GraphQuickCard QWidget#GraphQuickX0Row QLabel#GraphQuickX0Label { "
        "color: %4; font-size: 11px; font-weight: 700; letter-spacing: 0.2px; padding-left: 4px; }"
        "QFrame#GraphQuickCard QToolButton { background-color: rgba(255,255,255,0.14); "
        "border: 1px solid rgba(255,255,255,0.22); border-radius: 8px; min-width: 34px; min-height: 34px; "
        "color: %3; font-weight: 700; font-size: 13px; }"
        "QFrame#GraphQuickCard QToolButton:hover { background-color: %7; }"
        "QFrame#GraphQuickCard QDoubleSpinBox { background-color: %2; color: %3; "
        "border: 1px solid %6; border-radius: 8px; padding: 3px 6px; min-height: 28px; }")
                      .arg(bg, page, text, sub,
                           NoteChrome::rgbaCss(NoteChrome::accent(), 110),
                           NoteChrome::rgbaCss(NoteChrome::border(), 80),
                           NoteChrome::rgbaCss(NoteChrome::accent(), 90)));
}

void GraphQuickActionPopup::rebuildAnalyseMenu() {
    if (!m_menuAnalyse)
        return;
    m_menuAnalyse->clear();
    const QString page = NoteChrome::panelBg().name(QColor::HexRgb);
    const QString text = NoteChrome::textPrimary().name(QColor::HexRgb);
    m_menuAnalyse->setStyleSheet(QStringLiteral(
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; "
        "border-radius: 8px; padding: 4px; }"
        "QMenu::item { padding: 8px 24px 8px 12px; border-radius: 6px; }"
        "QMenu::item:selected { background-color: %4; }")
                                     .arg(page, text,
                                          NoteChrome::rgbaCss(NoteChrome::accent(), 110),
                                          NoteChrome::rgbaCss(NoteChrome::accent(), 90)));
    connect(m_menuAnalyse->addAction(QStringLiteral("Nullstellen (N)")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("roots")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Extremstellen (H/T)")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("extrema")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Wendepunkte (W)")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("inflection")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Schnittpunkte (S)")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("intersections")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Ableitung f'")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("derivative")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("2. Ableitung f''")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("second_derivative")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Ableitungskurve")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("derivative_create")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Integral ∫")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("integral")); });
    m_menuAnalyse->addSeparator();
    connect(m_menuAnalyse->addAction(QStringLiteral("Ansicht: Analyse-Labels")), &QAction::triggered,
            this, [this]() { emit toggleRequested(QStringLiteral("view_labels")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Ansicht: Legend")), &QAction::triggered, this,
            [this]() { emit toggleRequested(QStringLiteral("view_legend")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Ansicht: Wertetabelle")), &QAction::triggered,
            this, [this]() { emit toggleRequested(QStringLiteral("view_vtable")); });
    connect(m_menuAnalyse->addAction(QStringLiteral("Ansicht: Graph-Gitter")), &QAction::triggered,
            this, [this]() { emit toggleRequested(QStringLiteral("view_grid")); });
}

void GraphQuickActionPopup::syncParamRow(GraphCanvasItem *item) {
    if (!m_paramRow || !m_paramSlider)
        return;
    if (!item) {
        m_paramRow->hide();
        return;
    }
    const GraphObject d = item->data();
    if (d.selectedFunction < 0 || d.selectedFunction >= d.functions.size()) {
        m_paramRow->hide();
        return;
    }
    GraphFunction f = d.functions[d.selectedFunction];
    if (f.paramName.isEmpty()) {
        const QString detected = NumericAnalysis::detectFreeParam(f.expression);
        if (detected.isEmpty()) {
            m_paramRow->hide();
            return;
        }
        f.paramName = detected;
    }
    m_syncingParam = true;
    if (m_paramLabel)
        m_paramLabel->setText(f.paramName);
    m_paramSlider->setProperty("pmin", f.paramMin);
    m_paramSlider->setProperty("pmax", f.paramMax);
    const double span = qMax(1e-9, f.paramMax - f.paramMin);
    const int sliderV =
        qBound(0, int(qRound(1000.0 * (f.paramValue - f.paramMin) / span)), 1000);
    m_paramSlider->setValue(sliderV);
    if (m_paramValue)
        m_paramValue->setText(QString::number(f.paramValue, 'g', 3));
    m_paramRow->show();
    m_syncingParam = false;
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
        syncParamRow(nullptr);
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
    syncParamRow(item);
    adjustSize();
}
