#include "blop_theme.h"

#include <QApplication>
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
}

void BlopTheme::setMode(Mode m) {
  if (m == m_mode)
    return;
  m_mode = m;
  QSettings(kSettingsOrg, kSettingsApp).setValue(kKeyMode, modeKey(m));
  emit themeChanged();
}

void BlopTheme::setAccent(Accent a) {
  if (a == m_accent)
    return;
  m_accent = a;
  QSettings(kSettingsOrg, kSettingsApp).setValue(kKeyAccent, accentKey(a));
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
