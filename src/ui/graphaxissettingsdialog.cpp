#include "graphaxissettingsdialog.h"
#include "blop_dialogs.h"
#include "tools/GraphCanvasItem.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace {
constexpr int kTickAuto = 0;
constexpr int kTickStep = 1;
constexpr int kTickCount = 2;
} // namespace

GraphAxisSettingsDialog::GraphAxisSettingsDialog(GraphCanvasItem *graph, QWidget *parent)
    : QDialog(parent), m_graph(graph) {
    setWindowTitle(tr("Koordinatensystem"));
    setModal(true);
    resize(380, 420);

    auto *v = new QVBoxLayout(this);
    v->addWidget(new QLabel(tr("Achsenbereich (Datenkoordinaten):"), this));

    auto *form = new QFormLayout();
    m_xMin = new QDoubleSpinBox(this);
    m_xMax = new QDoubleSpinBox(this);
    m_yMin = new QDoubleSpinBox(this);
    m_yMax = new QDoubleSpinBox(this);
    for (auto *s : {m_xMin, m_xMax, m_yMin, m_yMax}) {
        s->setDecimals(4);
        s->setRange(-1e6, 1e6);
        s->setSingleStep(0.5);
    }

    if (m_graph) {
        const GraphObject d = m_graph->data();
        m_xMin->setValue(d.xMin);
        m_xMax->setValue(d.xMax);
        m_yMin->setValue(d.yMin);
        m_yMax->setValue(d.yMax);
    }

    form->addRow(tr("x min"), m_xMin);
    form->addRow(tr("x max"), m_xMax);
    form->addRow(tr("y min"), m_yMin);
    form->addRow(tr("y max"), m_yMax);
    v->addLayout(form);

    auto *tickGrp = new QGroupBox(tr("Teilstriche / Skala"), this);
    auto *tickLay = new QFormLayout(tickGrp);

    auto addModeCombo = [](QComboBox *cb) {
        cb->addItem(tr("Automatisch"), kTickAuto);
        cb->addItem(tr("Fester Abstand"), kTickStep);
        cb->addItem(tr("Anzahl Intervalle"), kTickCount);
    };

    m_xTickMode = new QComboBox(this);
    addModeCombo(m_xTickMode);
    m_yTickMode = new QComboBox(this);
    addModeCombo(m_yTickMode);

    m_xTickStep = new QDoubleSpinBox(this);
    m_xTickStep->setDecimals(6);
    m_xTickStep->setRange(1e-9, 1e6);
    m_xTickStep->setSingleStep(0.1);
    m_yTickStep = new QDoubleSpinBox(this);
    m_yTickStep->setDecimals(6);
    m_yTickStep->setRange(1e-9, 1e6);
    m_yTickStep->setSingleStep(0.1);

    m_xTickCount = new QSpinBox(this);
    m_xTickCount->setRange(2, 32);
    m_yTickCount = new QSpinBox(this);
    m_yTickCount->setRange(2, 32);

    m_xStepRow = new QWidget(this);
    auto *xStepL = new QHBoxLayout(m_xStepRow);
    xStepL->setContentsMargins(0, 0, 0, 0);
    xStepL->addWidget(new QLabel(tr("Abstand x:"), this));
    xStepL->addWidget(m_xTickStep, 1);

    m_xCountRow = new QWidget(this);
    auto *xCountL = new QHBoxLayout(m_xCountRow);
    xCountL->setContentsMargins(0, 0, 0, 0);
    xCountL->addWidget(new QLabel(tr("Intervalle x:"), this));
    xCountL->addWidget(m_xTickCount, 1);

    m_yStepRow = new QWidget(this);
    auto *yStepL = new QHBoxLayout(m_yStepRow);
    yStepL->setContentsMargins(0, 0, 0, 0);
    yStepL->addWidget(new QLabel(tr("Abstand y:"), this));
    yStepL->addWidget(m_yTickStep, 1);

    m_yCountRow = new QWidget(this);
    auto *yCountL = new QHBoxLayout(m_yCountRow);
    yCountL->setContentsMargins(0, 0, 0, 0);
    yCountL->addWidget(new QLabel(tr("Intervalle y:"), this));
    yCountL->addWidget(m_yTickCount, 1);

    tickLay->addRow(tr("x-Achse"), m_xTickMode);
    tickLay->addRow(QString(), m_xStepRow);
    tickLay->addRow(QString(), m_xCountRow);
    tickLay->addRow(tr("y-Achse"), m_yTickMode);
    tickLay->addRow(QString(), m_yStepRow);
    tickLay->addRow(QString(), m_yCountRow);
    v->addWidget(tickGrp);

    if (m_graph) {
        const GraphObject d = m_graph->data();
        const int xm = qBound(0, d.xTickMode, 2);
        const int ym = qBound(0, d.yTickMode, 2);
        m_xTickMode->setCurrentIndex(xm);
        m_yTickMode->setCurrentIndex(ym);
        m_xTickStep->setValue(d.xTickStep > 0 ? d.xTickStep : 1.0);
        m_yTickStep->setValue(d.yTickStep > 0 ? d.yTickStep : 1.0);
        m_xTickCount->setValue(qBound(2, d.xTickCount, 32));
        m_yTickCount->setValue(qBound(2, d.yTickCount, 32));
    }

    connect(m_xTickMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GraphAxisSettingsDialog::onXTickModeChanged);
    connect(m_yTickMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &GraphAxisSettingsDialog::onYTickModeChanged);
    onXTickModeChanged(m_xTickMode->currentIndex());
    onYTickModeChanged(m_yTickMode->currentIndex());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    v->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &GraphAxisSettingsDialog::apply);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void GraphAxisSettingsDialog::onXTickModeChanged(int index) {
    Q_UNUSED(index);
    const int m = m_xTickMode->currentIndex();
    m_xStepRow->setVisible(m == kTickStep);
    m_xCountRow->setVisible(m == kTickCount);
}

void GraphAxisSettingsDialog::onYTickModeChanged(int index) {
    Q_UNUSED(index);
    const int m = m_yTickMode->currentIndex();
    m_yStepRow->setVisible(m == kTickStep);
    m_yCountRow->setVisible(m == kTickCount);
}

void GraphAxisSettingsDialog::apply() {
    if (!m_graph) {
        reject();
        return;
    }
    const double xmin = m_xMin->value();
    const double xmax = m_xMax->value();
    const double ymin = m_yMin->value();
    const double ymax = m_yMax->value();
    if (xmin >= xmax || ymin >= ymax) {
        BlopDialogs::notify(this, tr("Ungültiger Bereich"),
                            tr("Es muss gelten: x min < x max und y min < y max."));
        return;
    }
    GraphObject d = m_graph->toData();
    d.xMin = xmin;
    d.xMax = xmax;
    d.yMin = ymin;
    d.yMax = ymax;
    d.xTickMode = qBound(0, m_xTickMode->currentIndex(), 2);
    d.yTickMode = qBound(0, m_yTickMode->currentIndex(), 2);
    d.xTickStep = m_xTickStep->value();
    d.yTickStep = m_yTickStep->value();
    d.xTickCount = qBound(2, m_xTickCount->value(), 32);
    d.yTickCount = qBound(2, m_yTickCount->value(), 32);
    if (d.xTickMode == kTickStep && d.xTickStep <= 0.0) {
        BlopDialogs::notify(this, tr("Ungültig"),
                            tr("x-Schrittweite muss größer 0 sein."));
        return;
    }
    if (d.yTickMode == kTickStep && d.yTickStep <= 0.0) {
        BlopDialogs::notify(this, tr("Ungültig"),
                            tr("y-Schrittweite muss größer 0 sein."));
        return;
    }
    m_graph->fromData(d);
    m_graph->notifyGraphChanged();
    accept();
}
