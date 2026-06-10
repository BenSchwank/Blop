#pragma once

#include <QColor>
#include <QObject>
#include <QString>

class QApplication;

/// Central design-token + theme manager for Blop. Provides a Dark and a
/// Light palette over the same accent color (Blop purple by default) and
/// emits `themeChanged` whenever the user switches mode or accent so that
/// surfaces can re-apply their stylesheets without a full app restart.
///
/// Usage:
///   BlopTheme::instance().install();         // once in main() after QApplication
///   const QColor bg = BlopTheme::surfaceBackground();
///   widget->setStyleSheet(BlopTheme::cardQss());
///   connect(&BlopTheme::instance(), &BlopTheme::themeChanged,
///           this, &MyWidget::applyThemeRefresh);
///
/// Persistence: `QSettings("Blop", "BlopApp")` keys `ui/theme_mode` and
/// `ui/accent` (lowercase string identifiers).
class BlopTheme : public QObject {
  Q_OBJECT
public:
  enum class Mode { Dark, Light };
  Q_ENUM(Mode)

  enum class Accent { Purple, Blue, Green, Pink };
  Q_ENUM(Accent)

  static BlopTheme &instance();

  /// Reads persisted settings and initializes the theme. Safe to call once
  /// per process; subsequent calls are no-ops.
  void install();

  Mode mode() const { return m_mode; }
  void setMode(Mode m);

  Accent accent() const { return m_accent; }
  void setAccent(Accent a);

  bool isDark() const { return m_mode == Mode::Dark; }
  bool isLight() const { return m_mode == Mode::Light; }

  // ---------------------------------------------------------------------
  // Color tokens (return live values for the currently selected theme).
  // ---------------------------------------------------------------------

  static QColor surfaceBackground();    // page-level background
  static QColor surfaceBase();          // cards / panels (default surface)
  static QColor surfaceElevated();      // overlay sheets, dialogs
  static QColor surfaceMuted();         // input fields, secondary surfaces
  static QColor surfaceInverse();       // contrast against background

  static QColor textPrimary();
  static QColor textSecondary();
  static QColor textTertiary();
  static QColor textOnAccent();         // text painted on accent fill

  static QColor accentPrimary();        // brand accent (Blop purple etc.)
  static QColor accentHover();
  static QColor accentPressed();
  static QColor accentSubtle();         // ~15% alpha tint
  static QColor accentBorder();         // ~42% alpha for outlines

  static QColor borderDefault();
  static QColor borderSubtle();
  static QColor borderStrong();

  static QColor shadowColor();
  static QColor scrimColor();           // backdrop behind modals

  // ---------------------------------------------------------------------
  // Spacing / radius / elevation tokens (px values, NOT dp -- callers
  // should multiply by UiScale::dp() where Android scaling matters).
  // ---------------------------------------------------------------------

  static constexpr int s4 = 4;
  static constexpr int s8 = 8;
  static constexpr int s12 = 12;
  static constexpr int s16 = 16;
  static constexpr int s24 = 24;
  static constexpr int s32 = 32;

  static constexpr int r8 = 8;
  static constexpr int r12 = 12;
  static constexpr int r16 = 16;
  static constexpr int r20 = 20;
  static constexpr int r24 = 24;

  // ---------------------------------------------------------------------
  // QSS bricks. Each function returns a stylesheet fragment built from
  // the live token values, so calling setStyleSheet(...) inside a
  // themeChanged slot is enough to retheme any widget.
  // ---------------------------------------------------------------------

  /// Card surface: background, subtle border, 16px radius.
  static QString cardQss(const QString &selectorObjectName = QString());
  /// Bottom-sheet surface: rounded top corners only, larger radius.
  static QString bottomSheetQss(const QString &selectorObjectName = QString());
  /// Primary filled button (accent background, on-accent text).
  static QString primaryButtonQss();
  /// Secondary (outline) button.
  static QString secondaryButtonQss();
  /// Ghost / tertiary text-only button.
  static QString tertiaryButtonQss();
  /// In-window menu look (matches blopWebMenuStyleSheet historical style
  /// in dark mode; auto-adapts to light mode).
  static QString menuFrameQss(const QString &selectorObjectName);
  static QString menuItemQss();
  static QString menuSeparatorQss();
  /// Text inputs (QLineEdit / QPlainTextEdit / QTextEdit).
  static QString inputQss();
  /// QTabWidget/QTabBar styled in M3 fashion.
  static QString tabBarQss();
  /// Backdrop / scrim color rule for a QWidget objectName.
  static QString scrimQss(const QString &objectName);

  /// v3.17.1: rewrite the well-known dark-surface hex values inside a raw
  /// QSS string to the live theme tokens. Used by the file-by-file
  /// migration so callers can keep their original stylesheet strings
  /// intact and just wrap them:
  ///   widget->setStyleSheet(BlopTheme::themed(R"(... raw qss ...)"));
  /// In Dark mode this is a near no-op (existing hex values already match
  /// the dark palette). In Light mode the helper swaps backgrounds,
  /// surfaces, borders and primary/secondary text to their light tokens
  /// and inverts subtle `rgba(255,255,255,X)` overlays to
  /// `rgba(0,0,0,X)` so they remain visible on light surfaces.
  /// Brand/accent colors (#7C5CFC, #5E5CE6, #4285F4 etc.), error red,
  /// success green and graph plot colors are NOT touched.
  static QString themed(const QString &rawQss);

signals:
  /// Emitted on Dark/Light mode change AND on accent change. Callers
  /// that need to re-apply stylesheets should always connect to this.
  void themeChanged();
  /// Emitted only on accent change (subset of themeChanged). Useful for
  /// surfaces that only depend on the accent (e.g. brand-only buttons).
  void accentChanged();

private:
  BlopTheme();
  static QString modeKey(Mode m);
  static Mode modeFromKey(const QString &k);
  static QString accentKey(Accent a);
  static Accent accentFromKey(const QString &k);

  Mode m_mode{Mode::Dark};
  Accent m_accent{Accent::Purple};
  bool m_installed{false};
};
