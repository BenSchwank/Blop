#include "blop_theme.h"

#include "UIStyles.h"

#include <QApplication>
#include <QRegularExpression>
#include <QSettings>

namespace {
constexpr const char *kSettingsOrg = "Blop";
constexpr const char *kSettingsApp = "BlopApp";
constexpr const char *kKeyMode = "ui/theme_mode";
constexpr const char *kKeyAccent = "ui/accent";

// Accent palettes. Each tuple is (primary, hover, pressed). The accent
// border / subtle tints are derived at call-time via setAlpha to avoid
// repeating per-accent rows.
struct AccentPalette {
  QColor primary;
  QColor hover;
  QColor pressed;
};

AccentPalette paletteFor(BlopTheme::Accent a) {
  switch (a) {
  case BlopTheme::Accent::Blue:
    return {QColor(0x4C, 0x8B, 0xF5), QColor(0x6BA0F8), QColor(0x3B72D8)};
  case BlopTheme::Accent::Green:
    return {QColor(0x35, 0xC4, 0x8A), QColor(0x4DD49E), QColor(0x2BAA75)};
  case BlopTheme::Accent::Pink:
    return {QColor(0xF0, 0x66, 0xB4), QColor(0xF581C2), QColor(0xD24E9A)};
  case BlopTheme::Accent::Purple:
  default:
    return {QColor(0x7C, 0x5C, 0xFC), QColor(0x957AFF), QColor(0x6443E0)};
  }
}

QString rgba(const QColor &c) {
  return QStringLiteral("rgba(%1,%2,%3,%4)")
      .arg(c.red())
      .arg(c.green())
      .arg(c.blue())
      .arg(QString::number(c.alphaF(), 'f', 3));
}

QString hex(const QColor &c) { return c.name(QColor::HexRgb); }
} // namespace

BlopTheme &BlopTheme::instance() {
  static BlopTheme theme;
  return theme;
}

BlopTheme::BlopTheme() = default;

void BlopTheme::install() {
  if (m_installed)
    return;
  m_installed = true;
  QSettings s(kSettingsOrg, kSettingsApp);
  m_mode = modeFromKey(s.value(kKeyMode, QStringLiteral("dark")).toString());
  m_accent =
      accentFromKey(s.value(kKeyAccent, QStringLiteral("purple")).toString());
  // Push the initial palette into UIStyles so existing callers (~42 sites)
  // see the right colors before any widget construction. Re-pushed on every
  // themeChanged signal below.
  UIStyles::refresh();
  QObject::connect(this, &BlopTheme::themeChanged, this,
                   []() { UIStyles::refresh(); });
}

void BlopTheme::setMode(Mode m) {
  if (m == m_mode)
    return;
  m_mode = m;
  clearThemedCache();
  QSettings(kSettingsOrg, kSettingsApp).setValue(kKeyMode, modeKey(m));
  emit themeChanged();
}

void BlopTheme::setAccent(Accent a) {
  if (a == m_accent)
    return;
  m_accent = a;
  clearThemedCache();
  QSettings(kSettingsOrg, kSettingsApp).setValue(kKeyAccent, accentKey(a));
  emit accentChanged();
  emit themeChanged();
}

QString BlopTheme::modeKey(Mode m) {
  return m == Mode::Light ? QStringLiteral("light") : QStringLiteral("dark");
}

BlopTheme::Mode BlopTheme::modeFromKey(const QString &k) {
  return k.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0
             ? Mode::Light
             : Mode::Dark;
}

QString BlopTheme::accentKey(Accent a) {
  switch (a) {
  case Accent::Blue:
    return QStringLiteral("blue");
  case Accent::Green:
    return QStringLiteral("green");
  case Accent::Pink:
    return QStringLiteral("pink");
  case Accent::Purple:
  default:
    return QStringLiteral("purple");
  }
}

BlopTheme::Accent BlopTheme::accentFromKey(const QString &k) {
  if (k.compare(QStringLiteral("blue"), Qt::CaseInsensitive) == 0)
    return Accent::Blue;
  if (k.compare(QStringLiteral("green"), Qt::CaseInsensitive) == 0)
    return Accent::Green;
  if (k.compare(QStringLiteral("pink"), Qt::CaseInsensitive) == 0)
    return Accent::Pink;
  return Accent::Purple;
}

// =====================================================================
// Color tokens
// =====================================================================

QColor BlopTheme::surfaceBackground() {
  return instance().isDark() ? QColor(0x0B, 0x0B, 0x1A) : QColor(0xF5, 0xF4, 0xF8);
}

QColor BlopTheme::surfaceBase() {
  return instance().isDark() ? QColor(0x14, 0x12, 0x1F) : QColor(0xFF, 0xFF, 0xFF);
}

QColor BlopTheme::surfaceElevated() {
  // Dark uses a slightly brighter card than the page; light uses pure white
  // so drop-shadows give the elevation cue instead of a shade change.
  return instance().isDark() ? QColor(0x1A, 0x17, 0x2A) : QColor(0xFF, 0xFF, 0xFF);
}

QColor BlopTheme::surfaceMuted() {
  return instance().isDark() ? QColor(0x21, 0x1E, 0x33) : QColor(0xEC, 0xEB, 0xF1);
}

QColor BlopTheme::surfaceInverse() {
  return instance().isDark() ? QColor(0xF5, 0xF4, 0xF8) : QColor(0x14, 0x12, 0x1F);
}

QColor BlopTheme::textPrimary() {
  return instance().isDark() ? QColor(0xE8, 0xE4, 0xFF) : QColor(0x1A, 0x17, 0x26);
}

QColor BlopTheme::textSecondary() {
  return instance().isDark() ? QColor(0xB4, 0xAE, 0xD4) : QColor(0x52, 0x51, 0x6B);
}

QColor BlopTheme::textTertiary() {
  return instance().isDark() ? QColor(0x78, 0x73, 0x95) : QColor(0x80, 0x7F, 0x9A);
}

QColor BlopTheme::textOnAccent() { return QColor(0xFF, 0xFF, 0xFF); }

QColor BlopTheme::accentPrimary() {
  return paletteFor(instance().m_accent).primary;
}

QColor BlopTheme::accentHover() {
  return paletteFor(instance().m_accent).hover;
}

QColor BlopTheme::accentPressed() {
  return paletteFor(instance().m_accent).pressed;
}

QColor BlopTheme::accentSubtle() {
  QColor c = accentPrimary();
  c.setAlpha(instance().isDark() ? 38 : 28); // ~15% dark / 11% light
  return c;
}

QColor BlopTheme::accentBorder() {
  QColor c = accentPrimary();
  c.setAlpha(instance().isDark() ? 107 : 80); // 42% / 31%
  return c;
}

QColor BlopTheme::borderDefault() {
  return instance().isDark() ? QColor(255, 255, 255, 28)
                             : QColor(26, 23, 38, 31);
}

QColor BlopTheme::borderSubtle() {
  return instance().isDark() ? QColor(255, 255, 255, 18)
                             : QColor(26, 23, 38, 18);
}

QColor BlopTheme::borderStrong() {
  return instance().isDark() ? QColor(255, 255, 255, 64)
                             : QColor(26, 23, 38, 64);
}

QColor BlopTheme::shadowColor() {
  return instance().isDark() ? QColor(0, 0, 0, 180) : QColor(26, 23, 38, 64);
}

QColor BlopTheme::scrimColor() { return QColor(0, 0, 0, 115); /* 45% */ }

// =====================================================================
// QSS bricks
// =====================================================================

QString BlopTheme::cardQss(const QString &selectorObjectName) {
  const QString sel = selectorObjectName.isEmpty()
                          ? QStringLiteral("QWidget")
                          : QStringLiteral("QWidget#%1").arg(selectorObjectName);
  return QStringLiteral(
             "%1 {"
             "  background: %2;"
             "  border: 1px solid %3;"
             "  border-radius: %4px;"
             "}")
      .arg(sel, hex(surfaceElevated()), rgba(borderDefault()),
           QString::number(r16));
}

QString BlopTheme::bottomSheetQss(const QString &selectorObjectName) {
  const QString sel = selectorObjectName.isEmpty()
                          ? QStringLiteral("QWidget")
                          : QStringLiteral("QWidget#%1").arg(selectorObjectName);
  return QStringLiteral(
             "%1 {"
             "  background: %2;"
             "  border-top: 1px solid %3;"
             "  border-left: 1px solid %3;"
             "  border-right: 1px solid %3;"
             "  border-top-left-radius: %4px;"
             "  border-top-right-radius: %4px;"
             "  border-bottom-left-radius: 0px;"
             "  border-bottom-right-radius: 0px;"
             "}")
      .arg(sel, hex(surfaceElevated()), rgba(borderDefault()),
           QString::number(r24));
}

QString BlopTheme::primaryButtonQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: %1;"
             "  color: %2;"
             "  border: none;"
             "  border-radius: %3px;"
             "  padding: 10px 18px;"
             "  font-weight: 600;"
             "}"
             "QPushButton:hover { background: %4; }"
             "QPushButton:pressed { background: %5; }"
             "QPushButton:disabled { background: %6; color: %7; }")
      .arg(hex(accentPrimary()), hex(textOnAccent()), QString::number(r12),
           hex(accentHover()), hex(accentPressed()), rgba(surfaceMuted()),
           rgba(textTertiary()));
}

QString BlopTheme::secondaryButtonQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent;"
             "  color: %1;"
             "  border: 1px solid %2;"
             "  border-radius: %3px;"
             "  padding: 10px 18px;"
             "  font-weight: 600;"
             "}"
             "QPushButton:hover { background: %4; }"
             "QPushButton:pressed { background: %5; }")
      .arg(hex(textPrimary()), rgba(accentBorder()), QString::number(r12),
           rgba(accentSubtle()), rgba(borderDefault()));
}

QString BlopTheme::tertiaryButtonQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent;"
             "  color: %1;"
             "  border: none;"
             "  border-radius: %2px;"
             "  padding: 8px 14px;"
             "  font-weight: 500;"
             "}"
             "QPushButton:hover { background: %3; }"
             "QPushButton:pressed { background: %4; }")
      .arg(hex(textPrimary()), QString::number(r8), rgba(accentSubtle()),
           rgba(borderSubtle()));
}

QString BlopTheme::menuFrameQss(const QString &selectorObjectName) {
  return QStringLiteral(
             "QFrame#%1 {"
             "  background: %2;"
             "  border: 1px solid %3;"
             "  border-radius: %4px;"
             "}")
      .arg(selectorObjectName, hex(surfaceElevated()), rgba(accentBorder()),
           QString::number(r12));
}

QString BlopTheme::menuItemQss() {
  return QStringLiteral(
             "QPushButton {"
             "  background: transparent;"
             "  color: %1;"
             "  border: none;"
             "  text-align: left;"
             "  padding: 10px 22px;"
             "  font-size: 13px;"
             "  font-weight: 500;"
             "  border-radius: %2px;"
             "}"
             "QPushButton:hover { background: %3; }"
             "QPushButton:pressed { background: %4; }")
      .arg(hex(textPrimary()), QString::number(r8), rgba(accentSubtle()),
           rgba(borderSubtle()));
}

QString BlopTheme::menuSeparatorQss() {
  return QStringLiteral(
             "QFrame {"
             "  background: %1;"
             "  border: none;"
             "  max-height: 1px;"
             "  min-height: 1px;"
             "  margin: 6px 12px;"
             "}")
      .arg(rgba(borderDefault()));
}

QString BlopTheme::inputQss() {
  return QStringLiteral(
             "QLineEdit, QPlainTextEdit, QTextEdit {"
             "  background: %1;"
             "  color: %2;"
             "  border: 1px solid %3;"
             "  border-radius: %4px;"
             "  padding: 8px 12px;"
             "  selection-background-color: %5;"
             "  selection-color: %6;"
             "}"
             "QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus {"
             "  border: 1px solid %7;"
             "}")
      .arg(hex(surfaceMuted()), hex(textPrimary()), rgba(borderDefault()),
           QString::number(r12), hex(accentPrimary()), hex(textOnAccent()),
           hex(accentPrimary()));
}

QString BlopTheme::tabBarQss() {
  return QStringLiteral(
             "QTabBar::tab {"
             "  background: transparent;"
             "  color: %1;"
             "  padding: 8px 18px;"
             "  border-bottom: 2px solid transparent;"
             "  font-weight: 500;"
             "}"
             "QTabBar::tab:selected {"
             "  color: %2;"
             "  border-bottom: 2px solid %3;"
             "}"
             "QTabBar::tab:hover { color: %4; }"
             "QTabWidget::pane { border: none; background: transparent; }")
      .arg(hex(textSecondary()), hex(textPrimary()), hex(accentPrimary()),
           hex(textPrimary()));
}

QString BlopTheme::scrimQss(const QString &objectName) {
  return QStringLiteral(
             "QWidget#%1 { background: %2; }")
      .arg(objectName, rgba(scrimColor()));
}

// =====================================================================
// themed() -- bulk-replace dark-surface hex values in a raw QSS string
// =====================================================================
namespace {
// Source -> target-color-getter. Ordered LONGEST-FIRST so the 6-char
// forms always match before any 3-char prefix (we never use 3-char
// hex in this map but it's still safer). Case-insensitive replace.
struct SurfaceHex {
  const char *from;
  QColor (*to)();
};

// Backgrounds (page-level)
const SurfaceHex kBackgroundHex[] = {
    {"#0B0B1A", BlopTheme::surfaceBackground},
    {"#0D0B14", BlopTheme::surfaceBackground},
    {"#0D0D12", BlopTheme::surfaceBackground},
    {"#0F111A", BlopTheme::surfaceBase}, // sidebar/header chrome
};

// Cards / sidebar / panel surfaces
const SurfaceHex kSurfaceBaseHex[] = {
    {"#14121F", BlopTheme::surfaceBase},
    {"#1A1A24", BlopTheme::surfaceBase},
    {"#1A1B2E", BlopTheme::surfaceBase},
    {"#1E1E1E", BlopTheme::surfaceBase},
    {"#1E1E2E", BlopTheme::surfaceBase},
    {"#161423", BlopTheme::surfaceBase},
};

// Elevated interactive surfaces (hover/list rows)
const SurfaceHex kSurfaceElevatedHex[] = {
    {"#1A1829", BlopTheme::surfaceElevated},
    {"#1C1A29", BlopTheme::surfaceElevated},
    {"#1C1A2A", BlopTheme::surfaceElevated},
};

// Inputs / muted backgrounds
const SurfaceHex kSurfaceMutedHex[] = {
    {"#252526", BlopTheme::surfaceMuted},
    {"#2A2640", BlopTheme::surfaceMuted},
    {"#262237", BlopTheme::surfaceMuted},
    {"#2d2b42", BlopTheme::surfaceMuted},
};

// Border default
const SurfaceHex kBorderHex[] = {
    {"#201E2E", BlopTheme::borderDefault},
    {"#2C2940", BlopTheme::borderDefault},
    {"#3A3550", BlopTheme::borderDefault},
};

// Text primary (high-contrast on dark)
const SurfaceHex kTextPrimaryHex[] = {
    {"#E0E0E0", BlopTheme::textPrimary},
    {"#E8E4FF", BlopTheme::textPrimary},
    {"#ECEEFD", BlopTheme::textPrimary},
    {"#F2F1FF", BlopTheme::textPrimary},
    {"#F4F2FF", BlopTheme::textPrimary},
    {"#F4F5FB", BlopTheme::textPrimary},
    {"#DCDCFF", BlopTheme::textPrimary},
    {"#E6E4FF", BlopTheme::textPrimary},
    {"#E8E8FF", BlopTheme::textPrimary},
    {"#E8E6F4", BlopTheme::textPrimary},
    {"#E8EAFF", BlopTheme::textPrimary},
    {"#EEF4FF", BlopTheme::textPrimary},
    {"#F0EEFF", BlopTheme::textPrimary},
    {"#E0DBFF", BlopTheme::textPrimary},
};

// Text secondary / tertiary (muted)
const SurfaceHex kTextSecondaryHex[] = {
    {"#A09FB8", BlopTheme::textSecondary},
    {"#A0A0C8", BlopTheme::textSecondary},
    {"#A7ACBB", BlopTheme::textSecondary},
    {"#C8C4E8", BlopTheme::textSecondary},
    {"#C8CDDC", BlopTheme::textSecondary},
    {"#D6DDF4", BlopTheme::textSecondary},
    {"#D8DEF2", BlopTheme::textSecondary},
    {"#D8DBE8", BlopTheme::textSecondary},
    {"#D8D5FF", BlopTheme::textSecondary},
    {"#DDD8FF", BlopTheme::textSecondary},
    {"#D8D4F5", BlopTheme::textSecondary},
    {"#A8B0DA", BlopTheme::textSecondary},
    {"#888888", BlopTheme::textSecondary},
    {"#aaaaaa", BlopTheme::textSecondary},
};

// 3-char text/border hex (apply LAST so longer hex doesn't get truncated).
// Common QSS shorthand colors used across the codebase as text or border
// hints. #FFF/white intentionally NOT mapped -- it's frequently the text
// color on accent buttons where it should stay legible in both themes.
const SurfaceHex kTextShortHex[] = {
    {"#DDD", BlopTheme::textPrimary},
    {"#ddd", BlopTheme::textPrimary},
    {"#CCC", BlopTheme::textPrimary},
    {"#ccc", BlopTheme::textPrimary},
    {"#BBB", BlopTheme::textSecondary},
    {"#bbb", BlopTheme::textSecondary},
    {"#AAA", BlopTheme::textSecondary},
    {"#aaa", BlopTheme::textSecondary},
    {"#999", BlopTheme::textSecondary},
    {"#888", BlopTheme::textSecondary},
    {"#777", BlopTheme::textTertiary},
    {"#666", BlopTheme::textTertiary},
    {"#555", BlopTheme::borderDefault},
    {"#444", BlopTheme::borderDefault},
    {"#333", BlopTheme::borderDefault},
};
} // namespace

// =====================================================================
// Typography (M3 type scale)
// =====================================================================
namespace {
// Conservative sizes — chosen to be close to existing hardcoded values
// in the codebase so adopting tokens causes minimal layout shift. Real
// M3 spec sizes are larger; iterate later once layouts have been
// audited surface by surface.
struct TypeSpec {
  int pixelSize;
  int weight; // QFont::Weight raw value (100..900)
  qreal letterSpacingPx;
};

TypeSpec specFor(BlopTheme::TextRole r) {
  using TR = BlopTheme::TextRole;
  switch (r) {
  case TR::DisplayLarge:
    return {44, QFont::Normal, 0.0};
  case TR::DisplayMedium:
    return {36, QFont::Normal, 0.0};
  case TR::DisplaySmall:
    return {28, QFont::Normal, 0.0};
  case TR::HeadlineLarge:
    return {24, QFont::DemiBold, 0.0};
  case TR::HeadlineMedium:
    return {20, QFont::DemiBold, 0.0};
  case TR::HeadlineSmall:
    return {17, QFont::DemiBold, 0.0};
  case TR::TitleLarge:
    return {16, QFont::DemiBold, 0.0};
  case TR::TitleMedium:
    return {14, QFont::DemiBold, 0.15};
  case TR::TitleSmall:
    return {13, QFont::Medium, 0.1};
  case TR::BodyLarge:
    return {15, QFont::Normal, 0.15};
  case TR::BodyMedium:
    return {14, QFont::Normal, 0.25};
  case TR::BodySmall:
    return {12, QFont::Normal, 0.4};
  case TR::LabelLarge:
    return {13, QFont::Medium, 0.1};
  case TR::LabelMedium:
    return {12, QFont::Medium, 0.5};
  case TR::LabelSmall:
    return {11, QFont::Medium, 0.5};
  }
  return {14, QFont::Normal, 0.0};
}
} // namespace

QFont BlopTheme::font(TextRole role) {
  const TypeSpec s = specFor(role);
  QFont f = qApp ? qApp->font() : QFont();
  f.setPixelSize(s.pixelSize);
  f.setWeight(static_cast<QFont::Weight>(s.weight));
  if (s.letterSpacingPx != 0.0)
    f.setLetterSpacing(QFont::AbsoluteSpacing, s.letterSpacingPx);
  return f;
}

QString BlopTheme::typeQss(TextRole role) {
  const TypeSpec s = specFor(role);
  return QStringLiteral("font-size: %1px; font-weight: %2;")
      .arg(s.pixelSize)
      .arg(s.weight);
}

namespace {
// v3.17.5: memoization for BlopTheme::themed(). Cleared on every theme/
// accent change via clearThemedCache() invoked from setMode/setAccent.
QHash<QPair<int, QString>, QString> s_themedCache;
} // namespace

void BlopTheme::clearThemedCache() { s_themedCache.clear(); }

QString BlopTheme::themed(const QString &rawQss) {
  // Dark mode: the existing hardcoded hex values ARE the dark palette;
  // shipping them unchanged keeps zero visual diff for the bulk of users.
  if (instance().isDark())
    return rawQss;

  // Memoize per (mode, accent, rawQss). Light-mode does ~50
  // case-insensitive QString::replace + 1 regex per call -- with 16 calls
  // per Settings-Dialog open that's ~800 replaces over multi-KB strings.
  // The cache is keyed on the full raw string (safe against dynamic
  // arg()-built QSS), and invalidated whenever the user flips theme or
  // accent.
  const int key = static_cast<int>(instance().m_mode) * 10 +
                  static_cast<int>(instance().m_accent);
  const auto cacheKey = qMakePair(key, rawQss);
  if (auto it = s_themedCache.constFind(cacheKey);
      it != s_themedCache.constEnd())
    return *it;

  QString out = rawQss;

  // Phase 1: invert subtle white-on-dark overlays to black-on-light so
  // hover/border tints remain visible. Uses regex to capture the alpha
  // and rebuild with the same alpha but inverted color channel.
  // Matches both `rgba(255,255,255,0.06)` and `rgba(255, 255, 255, 0.06)`.
  static const QRegularExpression whiteTintRe(
      QStringLiteral(R"(rgba\(\s*255\s*,\s*255\s*,\s*255\s*,\s*([0-9.]+)\s*\))"));
  out.replace(whiteTintRe, QStringLiteral("rgba(0,0,0,\\1)"));

  // Phase 2: surface/text hex swap. Longer-first lists run before
  // shorter so 6-char forms always win over 3-char prefixes.
  auto applyMap = [&out](const SurfaceHex *map, size_t n) {
    for (size_t i = 0; i < n; ++i) {
      out.replace(QLatin1String(map[i].from),
                  map[i].to().name(QColor::HexRgb),
                  Qt::CaseInsensitive);
    }
  };
  applyMap(kBackgroundHex, sizeof(kBackgroundHex) / sizeof(SurfaceHex));
  applyMap(kSurfaceBaseHex, sizeof(kSurfaceBaseHex) / sizeof(SurfaceHex));
  applyMap(kSurfaceElevatedHex,
           sizeof(kSurfaceElevatedHex) / sizeof(SurfaceHex));
  applyMap(kSurfaceMutedHex, sizeof(kSurfaceMutedHex) / sizeof(SurfaceHex));
  applyMap(kBorderHex, sizeof(kBorderHex) / sizeof(SurfaceHex));
  applyMap(kTextPrimaryHex, sizeof(kTextPrimaryHex) / sizeof(SurfaceHex));
  applyMap(kTextSecondaryHex,
           sizeof(kTextSecondaryHex) / sizeof(SurfaceHex));
  // 3-char codes LAST so we don't truncate longer matches.
  applyMap(kTextShortHex, sizeof(kTextShortHex) / sizeof(SurfaceHex));

  s_themedCache.insert(cacheKey, out);
  return out;
}
