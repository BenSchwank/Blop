#include "helpdialog.h"

#include "blop_theme.h"
#include "blopstyle.h"
#include "uiscale.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QLabel *sectionTitle(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setStyleSheet(
      QStringLiteral("color: %1; font-size: 13px; font-weight: 700; "
                     "letter-spacing: 0.6px; background: transparent;")
          .arg(BlopStyle::paperInkMuted().name()));
  return l;
}

QLabel *bodyText(const QString &text, QWidget *parent) {
  auto *l = new QLabel(text, parent);
  l->setWordWrap(true);
  l->setTextFormat(Qt::RichText);
  l->setStyleSheet(
      QStringLiteral("color: %1; font-size: 14px; background: transparent;")
          .arg(BlopStyle::paperInk().name()));
  return l;
}

QWidget *shortcutRow(const QString &keys, const QString &what, QWidget *parent) {
  auto *row = new QWidget(parent);
  auto *h = new QHBoxLayout(row);
  h->setContentsMargins(0, 0, 0, 0);
  h->setSpacing(UiScale::dp(12));
  auto *k = new QLabel(keys, row);
  k->setMinimumWidth(UiScale::dp(130));
  k->setStyleSheet(
      QStringLiteral("color: %1; background: %2; border: 1px solid %3; "
                     "border-radius: 6px; padding: 3px 8px; font-size: 12px; "
                     "font-weight: 600;")
          .arg(BlopStyle::paperInk().name(),
               BlopStyle::paperChipBg().name(),
               BlopStyle::paperBorder().name()));
  auto *w = new QLabel(what, row);
  w->setStyleSheet(
      QStringLiteral("color: %1; font-size: 14px; background: transparent;")
          .arg(BlopStyle::paperInk().name()));
  h->addWidget(k, 0);
  h->addWidget(w, 1);
  return row;
}

} // namespace

HelpDialog::HelpDialog(const QString &versionText, QWidget *parent)
    : QDialog(parent) {
  setWindowTitle(QStringLiteral("Hilfe"));
  setProperty("blopForcePaper", true);
  setMinimumWidth(UiScale::dp(440));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(UiScale::dp(22), UiScale::dp(18), UiScale::dp(22),
                           UiScale::dp(16));
  root->setSpacing(UiScale::dp(12));

  auto *title = new QLabel(QStringLiteral("Hilfe & Erste Schritte"), this);
  title->setStyleSheet(
      QStringLiteral("color: %1; font-size: 18px; font-weight: 700; "
                     "background: transparent;")
          .arg(BlopStyle::paperInk().name()));
  root->addWidget(title);

  auto *scroll = new QScrollArea(this);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;}"));
  scroll->setMinimumHeight(UiScale::dp(300));
  auto *content = new QWidget(scroll);
  content->setStyleSheet(QStringLiteral("background: transparent;"));
  auto *col = new QVBoxLayout(content);
  col->setContentsMargins(0, 0, UiScale::dp(6), 0);
  col->setSpacing(UiScale::dp(10));

  col->addWidget(sectionTitle(QStringLiteral("ERSTE SCHRITTE"), content));
  col->addWidget(bodyText(
      QStringLiteral(
          "<b>1.</b> Mit <b>Neue Notiz</b> legst du eine Notiz an: "
          "<i>Unendlich</i> (freie Fläche), <i>DIN A4</i> (Seiten) oder "
          "<i>Struktur</i> (Dokument mit eingebetteten Notizen).<br>"
          "<b>2.</b> In der <b>Bibliothek</b> ordnest du Notizen in Ordnern, "
          "mit Tags, Favoriten und Farbmarkierungen. Rechtsklick öffnet das "
          "Menü (Umbenennen, Duplizieren, Papierkorb).<br>"
          "<b>3.</b> Gelöschtes landet im <b>Papierkorb</b> und kann "
          "wiederhergestellt werden.<br>"
          "<b>4.</b> Unter <b>Einstellungen → Cloud</b> verknüpfst du Google "
          "Drive, Nextcloud oder einen Sync-Ordner. Ohne Konto bleibt alles "
          "lokal auf diesem Gerät."),
      content));

  col->addWidget(sectionTitle(QStringLiteral("TASTENKÜRZEL"), content));
  struct Sc {
    const char *keys;
    const char *what;
  };
  static const Sc kShortcuts[] = {
      {"Strg + N", "Neue Notiz"},
      {"Strg + K", "Bibliothek durchsuchen"},
      {"Strg + 0", "Ganze Seite einpassen"},
      {"Strg + 9", "An Seitenbreite anpassen"},
      {"Strg + Umschalt + O", "Werkzeug-Eigenschaften"},
      {"Strg + Umschalt + J", "Werkzeugleiste umschalten"},
      {"1 – 9", "Seiten-Reiter wählen"},
      {"Entf", "Auswahl in den Papierkorb"},
      {"Esc", "Dialog / Auswahl schließen"},
  };
  for (const Sc &s : kShortcuts)
    col->addWidget(shortcutRow(QString::fromUtf8(s.keys),
                               QString::fromUtf8(s.what), content));

  col->addWidget(sectionTitle(QStringLiteral("TIPPS"), content));
  col->addWidget(bodyText(
      QStringLiteral(
          "Notizen werden automatisch gespeichert (Intervall in den "
          "Einstellungen → Werkzeuge). Bei einer beschädigten Datei stellt "
          "Blop sie aus der letzten Sicherung (<i>.bak</i>) wieder her.<br>"
          "Eigene Werkzeug-Tasten legst du unter Einstellungen → Tastatur "
          "fest."),
      content));
  col->addStretch(1);
  scroll->setWidget(content);
  root->addWidget(scroll, 1);

  auto *ver = new QLabel(versionText, this);
  ver->setStyleSheet(
      QStringLiteral("color: %1; font-size: 12px; background: transparent;")
          .arg(BlopStyle::paperInkMuted().name()));
  root->addWidget(ver);

  auto *row = new QHBoxLayout();
  row->setSpacing(UiScale::dp(8));
  auto *replay = new QPushButton(QStringLiteral("Einführung wiederholen"), this);
  replay->setStyleSheet(BlopTheme::secondaryButtonQss());
  auto *settings = new QPushButton(QStringLiteral("Einstellungen"), this);
  settings->setStyleSheet(BlopTheme::secondaryButtonQss());
  auto *close = new QPushButton(QStringLiteral("Schließen"), this);
  close->setStyleSheet(BlopTheme::primaryButtonQss());
  close->setDefault(true);
  row->addWidget(replay);
  row->addWidget(settings);
  row->addStretch(1);
  row->addWidget(close);
  root->addLayout(row);

  connect(close, &QPushButton::clicked, this, &QDialog::accept);
  connect(replay, &QPushButton::clicked, this, [this]() {
    accept();
    emit replayOnboardingRequested();
  });
  connect(settings, &QPushButton::clicked, this, [this]() {
    accept();
    emit openSettingsRequested();
  });
}
