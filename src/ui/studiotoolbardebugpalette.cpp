#include "studiotoolbardebugpalette.h"

#include "moderntoolbar.h"
#include "notechrome.h"
#include "ToolMode.h"
#include "tools/ToolManager.h"
#include "uiscale.h"

#include <QButtonGroup>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

class GlyphChip : public QPushButton {
public:
  GlyphChip(const QString &glyph, const QString &label, QWidget *parent)
      : QPushButton(parent), m_glyph(glyph) {
    setText(label);
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(UiScale::dp(52));
    setMinimumWidth(UiScale::dp(72));
  }

protected:
  void paintEvent(QPaintEvent *e) override {
    QPushButton::paintEvent(e);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor fg = isChecked() ? NoteChrome::accent()
                                  : NoteChrome::textPrimary();
    p.save();
    p.translate(width() / 2.0, UiScale::dp(16));
    p.scale(UiScale::dp(18) / 64.0, UiScale::dp(18) / 64.0);
    p.translate(-32, -32);
    blopDrawToolbarGlyph64(&p, m_glyph, fg);
    p.restore();
  }

private:
  QString m_glyph;
};

} // namespace

StudioToolbarDebugPalette::StudioToolbarDebugPalette(ModernToolbar *toolbar,
                                                     QWidget *parent)
    : QWidget(parent), m_toolbar(toolbar) {
  setObjectName(QStringLiteral("StudioToolbarDebugPalette"));
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet(QStringLiteral(
      "QWidget#StudioToolbarDebugPalette {"
      "  background: rgba(28,30,36,230);"
      "  border: 1px solid rgba(255,255,255,0.12);"
      "  border-radius: 12px;"
      "}"
      "QLabel { color: #E8EAEE; background: transparent; }"
      "QPushButton {"
      "  background: rgba(255,255,255,0.06); color: #E8EAEE;"
      "  border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
      "  padding: 22px 6px 6px 6px; font-size: 10px;"
      "}"
      "QPushButton:checked {"
      "  background: rgba(91,157,255,0.28);"
      "  border-color: rgba(91,157,255,0.65);"
      "}"));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(10), UiScale::dp(10), UiScale::dp(10),
                           UiScale::dp(10));
  root->setSpacing(UiScale::dp(8));

  auto *title = new QLabel(QStringLiteral("Toolbar-Debug"), this);
  title->setStyleSheet(
      QStringLiteral("font-weight: 700; font-size: 12px; color: #F2F2F2;"));
  root->addWidget(title);

  auto *hint = new QLabel(
      QStringLiteral("Varianten A–D · Tools · Coming: Geodreieck"), this);
  hint->setWordWrap(true);
  hint->setStyleSheet(QStringLiteral("font-size: 10px; color: #9CA3AF;"));
  root->addWidget(hint);

  m_variantGroup = new QButtonGroup(this);
  m_variantGroup->setExclusive(true);
  auto *varRow = new QHBoxLayout();
  varRow->setSpacing(UiScale::dp(4));
  const struct {
    const char *label;
    ModernToolbar::StudioToolbarVariant v;
  } variants[] = {
      {"A Labels", ModernToolbar::StudioToolbarVariant::HorizontalLabeled},
      {"B Flat", ModernToolbar::StudioToolbarVariant::HorizontalFlat},
      {"C Radial", ModernToolbar::StudioToolbarVariant::ComplexRadial},
      {"D Vertikal", ModernToolbar::StudioToolbarVariant::VerticalGrid},
      {"Legacy", ModernToolbar::StudioToolbarVariant::Legacy},
  };
  for (const auto &entry : variants) {
    auto *b = new QPushButton(QString::fromLatin1(entry.label), this);
    b->setCheckable(true);
    b->setFixedHeight(UiScale::dp(28));
    b->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 4px 8px; font-size: 11px; font-weight: 600; }"));
    m_variantGroup->addButton(b, static_cast<int>(entry.v));
    varRow->addWidget(b);
    connect(b, &QPushButton::clicked, this, [this, entry]() {
      if (m_toolbar)
        m_toolbar->setStudioToolbarVariant(entry.v, true);
      emit variantChosen(static_cast<int>(entry.v));
    });
  }
  root->addLayout(varRow);

  auto *toolsTitle = new QLabel(QStringLiteral("Tools"), this);
  toolsTitle->setStyleSheet(
      QStringLiteral("font-weight: 600; font-size: 11px; margin-top: 4px;"));
  root->addWidget(toolsTitle);

  auto *grid = new QGridLayout();
  grid->setSpacing(UiScale::dp(4));
  const struct {
    ToolMode mode;
    const char *glyph;
    const char *label;
  } tools[] = {
      {ToolMode::Hand, "hand", "Hand"},
      {ToolMode::Pen, "pen", "Stift"},
      {ToolMode::Pencil, "pencil", "Bleistift"},
      {ToolMode::Highlighter, "highlighter", "Marker"},
      {ToolMode::Eraser, "eraser", "Radierer"},
      {ToolMode::Lasso, "lasso_loop", "Auswahl"},
      {ToolMode::Formula, "pi", "Formel"},
      {ToolMode::Ruler, "measure", "Messen"},
      {ToolMode::Molecule, "molecule", "Molekül"},
      {ToolMode::Shape, "rect", "Form"},
      {ToolMode::Text, "text", "Text"},
      {ToolMode::Image, "image", "Bild"},
  };
  int i = 0;
  for (const auto &t : tools) {
    auto *chip = new GlyphChip(QString::fromLatin1(t.glyph),
                               QString::fromLatin1(t.label), this);
    grid->addWidget(chip, i / 4, i % 4);
    connect(chip, &QPushButton::clicked, this, [this, mode = t.mode]() {
      if (!m_toolbar)
        return;
      ToolManager::instance().selectTool(mode);
      m_toolbar->setToolMode(mode);
    });
    ++i;
  }
  // Coming: set_square glyph preview (not a ToolMode yet).
  auto *coming = new GlyphChip(QStringLiteral("set_square"),
                               QStringLiteral("Geodreieck*"), this);
  coming->setEnabled(false);
  coming->setToolTip(QStringLiteral("Coming soon"));
  grid->addWidget(coming, i / 4, i % 4);
  root->addLayout(grid);

  setFixedWidth(UiScale::dp(340));
  refreshFromToolbar();
}

void StudioToolbarDebugPalette::refreshFromToolbar() {
  if (!m_toolbar || !m_variantGroup)
    return;
  const int id = static_cast<int>(m_toolbar->studioToolbarVariant());
  if (auto *b = m_variantGroup->button(id))
    b->setChecked(true);
}
