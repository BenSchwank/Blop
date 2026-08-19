#include "blopassistantengine.h"
#include "blopassistantoverlay.h"

#include <QAbstractNativeEventFilter>
#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QProcess>
#include <QShortcut>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QString sandboxDir() {
  const QString root =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString dir = root + QStringLiteral("/BlopAssistantSandbox");
  QDir().mkpath(dir);
  return dir;
}

QString notesListPath() { return sandboxDir() + QStringLiteral("/notes.txt"); }

QStringList loadNotes() {
  QStringList out;
  QFile f(notesListPath());
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return out;
  QTextStream in(&f);
  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    if (!line.isEmpty())
      out << line;
  }
  return out;
}

void saveNotes(const QStringList &notes) {
  QFile f(notesListPath());
  if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    return;
  QTextStream out(&f);
  for (const QString &n : notes)
    out << n << '\n';
}

void speak(const QString &text) {
  const QString t = text.trimmed();
  if (t.isEmpty())
    return;
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
  if (QProcess::startDetached(QStringLiteral("spd-say"),
                              {QStringLiteral("-l"), QStringLiteral("de"), t}))
    return;
  QProcess::startDetached(QStringLiteral("espeak"),
                          {QStringLiteral("-v"), QStringLiteral("de"), t});
#elif defined(Q_OS_WIN)
  const QString escaped =
      QString(t).replace(QLatin1Char('\''), QStringLiteral("''"));
  QProcess::startDetached(
      QStringLiteral("powershell"),
      {QStringLiteral("-NoProfile"), QStringLiteral("-WindowStyle"),
       QStringLiteral("Hidden"), QStringLiteral("-Command"),
       QStringLiteral("Add-Type -AssemblyName System.Speech; "
                      "(New-Object System.Speech.Synthesis.SpeechSynthesizer)"
                      ".Speak('%1')")
           .arg(escaped)});
#else
  Q_UNUSED(t);
#endif
}

bool launchApp(const QString &name) {
  const QString n = name.trimmed().toLower();
#ifdef Q_OS_WIN
  QString cmd = name;
  if (n == QLatin1String("chrome"))
    cmd = QStringLiteral("chrome");
  else if (n == QLatin1String("calculator"))
    cmd = QStringLiteral("calc");
  else if (n == QLatin1String("explorer"))
    cmd = QStringLiteral("explorer");
  else if (n == QLatin1String("terminal"))
    cmd = QStringLiteral("cmd");
  else if (n == QLatin1String("code"))
    cmd = QStringLiteral("code");
  return QProcess::startDetached(QStringLiteral("cmd"),
                                 {QStringLiteral("/c"), QStringLiteral("start"),
                                  QStringLiteral(""), cmd});
#else
  QStringList candidates;
  if (n == QLatin1String("chrome"))
    candidates << QStringLiteral("google-chrome")
               << QStringLiteral("google-chrome-stable")
               << QStringLiteral("chromium") << QStringLiteral("chrome");
  else if (n == QLatin1String("calculator"))
    candidates << QStringLiteral("gnome-calculator") << QStringLiteral("kcalc")
               << QStringLiteral("galculator");
  else if (n == QLatin1String("explorer"))
    candidates << QStringLiteral("xdg-open");
  else if (n == QLatin1String("terminal"))
    candidates << QStringLiteral("x-terminal-emulator")
               << QStringLiteral("gnome-terminal") << QStringLiteral("konsole");
  else if (n == QLatin1String("code"))
    candidates << QStringLiteral("code") << QStringLiteral("codium");
  else
    candidates << name;
  if (n == QLatin1String("explorer"))
    return QProcess::startDetached(QStringLiteral("xdg-open"),
                                   {QDir::homePath()});
  for (const QString &c : candidates) {
    if (QProcess::startDetached(c, {}))
      return true;
  }
  return false;
#endif
}

int runSelfTest() {
  int fails = 0;
  auto expect = [&](const QString &utterance, BlopAssistantAction action,
                    const QString &payload) {
    const BlopAssistantIntent i = BlopAssistantEngine::parse(utterance);
    if (i.action != action) {
      QTextStream(stderr) << "FAIL action: [" << utterance << "] got "
                          << int(i.action) << " expected " << int(action)
                          << '\n';
      ++fails;
      return;
    }
    QString got;
    if (action == BlopAssistantAction::CreateNote)
      got = i.title;
    else if (action == BlopAssistantAction::OpenUrl)
      got = i.url.host();
    else if (action == BlopAssistantAction::LaunchApp)
      got = i.appName;
    else
      got = i.query;
    if (!payload.isEmpty() &&
        !got.contains(payload, Qt::CaseInsensitive)) {
      QTextStream(stderr) << "FAIL payload: [" << utterance << "] got [" << got
                          << "] expected to contain [" << payload << "]\n";
      ++fails;
    }
  };

  expect(QStringLiteral("neue Notiz Einkauf"), BlopAssistantAction::CreateNote,
         QStringLiteral("Einkauf"));
  expect(QStringLiteral("Notiz Meeting: Agenda 10 Uhr"),
         BlopAssistantAction::CreateNote, QStringLiteral("Meeting"));
  expect(QStringLiteral("unendliche Notiz Skizze"),
         BlopAssistantAction::CreateNote, QStringLiteral("Skizze"));
  expect(QStringLiteral("öffne Physik"), BlopAssistantAction::OpenNote,
         QStringLiteral("Physik"));
  expect(QStringLiteral("suche Mathe"), BlopAssistantAction::SearchNotes,
         QStringLiteral("Mathe"));
  expect(QStringLiteral("Bibliothek"), BlopAssistantAction::ShowLibrary,
         QString());
  expect(QStringLiteral("Hilfe"), BlopAssistantAction::Help, QString());
  expect(QStringLiteral("öffne YouTube"), BlopAssistantAction::OpenUrl,
         QStringLiteral("youtube"));
  expect(QStringLiteral("starte calculator"), BlopAssistantAction::LaunchApp,
         QStringLiteral("calculator"));
  expect(QStringLiteral("suche im Web Qt"), BlopAssistantAction::OpenUrl,
         QStringLiteral("google"));

  const BlopAssistantIntent inf =
      BlopAssistantEngine::parse(QStringLiteral("unendliche Notiz Skizze"));
  if (!inf.infinite) {
    QTextStream(stderr) << "FAIL infinite flag\n";
    ++fails;
  }
  const BlopAssistantIntent yt =
      BlopAssistantEngine::parse(QStringLiteral("öffne YouTube"));
  if (!yt.needsConfirm) {
    QTextStream(stderr) << "FAIL confirm flag\n";
    ++fails;
  }

  if (fails == 0)
    QTextStream(stdout) << "blop-assistant-sandbox self-test: ok\n";
  else
    QTextStream(stderr) << "blop-assistant-sandbox self-test: " << fails
                        << " failed\n";
  return fails == 0 ? 0 : 1;
}

#ifdef Q_OS_WIN
class WinHotkeyFilter : public QAbstractNativeEventFilter {
public:
  explicit WinHotkeyFilter(BlopAssistantOverlay *pill) : m_pill(pill) {}
  bool nativeEventFilter(const QByteArray &, void *message,
                         qintptr *) override {
    auto *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && m_pill)
      m_pill->toggleExpanded();
    return false;
  }

private:
  BlopAssistantOverlay *m_pill{nullptr};
};
#endif

class Companion : public QObject {
public:
  explicit Companion(BlopAssistantOverlay *pill, QObject *parent = nullptr)
      : QObject(parent), m_pill(pill) {
    m_notes = loadNotes();
    if (m_notes.isEmpty())
      m_notes << QStringLiteral("Willkommen (Beispiel)");
    saveNotes(m_notes);

    connect(pill, &BlopAssistantOverlay::utteranceSubmitted, this,
            &Companion::onUtterance);
    connect(pill, &BlopAssistantOverlay::confirmAccepted, this,
            &Companion::onConfirm);
    connect(pill, &BlopAssistantOverlay::confirmRejected, this, [this]() {
      m_pending = {};
      m_pending.action = BlopAssistantAction::Unknown;
    });

    auto *tray = new QSystemTrayIcon(this);
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(124, 92, 252));
    p.setPen(Qt::NoPen);
    p.drawEllipse(8, 8, 48, 48);
    p.setBrush(QColor(91, 157, 255));
    p.drawEllipse(22, 22, 20, 20);
    p.end();
    tray->setIcon(QIcon(pm));
    tray->setToolTip(QStringLiteral("Blop Assistant"));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("Assistent öffnen"), pill, [pill]() {
      pill->setExpanded(true);
    });
    menu->addAction(QStringLiteral("Beenden"), qApp, &QApplication::quit);
    tray->setContextMenu(menu);
    connect(tray, &QSystemTrayIcon::activated, this,
            [pill](QSystemTrayIcon::ActivationReason reason) {
              if (reason == QSystemTrayIcon::Trigger)
                pill->toggleExpanded();
            });
    tray->show();
    m_pill->setStatus(
        QStringLiteral("Läuft im Hintergrund. Klick oder Ctrl+Shift+Leertaste."));
  }

private:
  QString uniqueName(const QString &title) const {
    QString base = title.trimmed();
    if (base.isEmpty())
      base = QStringLiteral("Neue Notiz");
    QString name = base;
    int n = 2;
    auto exists = [&](const QString &c) {
      return m_notes.contains(c, Qt::CaseInsensitive);
    };
    while (exists(name))
      name = base + QStringLiteral(" (%1)").arg(n++);
    return name;
  }

  bool openMatching(const QString &query, QString *opened) {
    const QString q = query.trimmed().toLower();
    int best = -1;
    int bestScore = 0;
    for (int i = 0; i < m_notes.size(); ++i) {
      const QString n = m_notes.at(i).toLower();
      int score = 0;
      if (n == q)
        score = 300;
      else if (n.startsWith(q))
        score = 200;
      else if (n.contains(q))
        score = 100;
      if (score > bestScore) {
        bestScore = score;
        best = i;
      }
    }
    if (best < 0)
      return false;
    *opened = m_notes.at(best);
    return true;
  }

  void onUtterance(const QString &text) {
    const BlopAssistantIntent intent = BlopAssistantEngine::parse(text);
    if (intent.needsConfirm) {
      m_pending = intent;
      m_pill->promptConfirm(intent.statusLine +
                            QStringLiteral("  — wie VoiceOS erst nach Bestätigung."));
      return;
    }
    runIntent(intent);
  }

  void onConfirm() {
    if (m_pending.action == BlopAssistantAction::Unknown)
      return;
    runIntent(m_pending);
    m_pending = {};
    m_pending.action = BlopAssistantAction::Unknown;
  }

  void runIntent(const BlopAssistantIntent &intent) {
    switch (intent.action) {
    case BlopAssistantAction::CreateNote: {
      const QString name = uniqueName(intent.title);
      m_notes.append(name);
      saveNotes(m_notes);
      QFile body(sandboxDir() + QLatin1Char('/') + name +
                 QStringLiteral(".txt"));
      if (body.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&body);
        out << name << '\n';
        if (!intent.body.isEmpty())
          out << intent.body << '\n';
      }
      m_pill->setStatus(
          QStringLiteral("Notiz „%1“ angelegt. %2 gespeichert.")
              .arg(name)
              .arg(m_notes.size()));
      speak(intent.spokenReply);
      break;
    }
    case BlopAssistantAction::OpenNote: {
      QString opened;
      if (openMatching(intent.query, &opened)) {
        m_pill->setStatus(QStringLiteral("Notiz „%1“ gefunden.").arg(opened));
        speak(intent.spokenReply);
      } else {
        m_pill->setStatus(
            QStringLiteral("Keine Notiz „%1“.").arg(intent.query));
      }
      break;
    }
    case BlopAssistantAction::SearchNotes: {
      int hits = 0;
      for (const QString &n : m_notes) {
        if (n.contains(intent.query, Qt::CaseInsensitive))
          ++hits;
      }
      m_pill->setStatus(
          QStringLiteral("%1 Treffer für „%2“.").arg(hits).arg(intent.query));
      speak(intent.spokenReply);
      break;
    }
    case BlopAssistantAction::ShowLibrary:
      m_pill->setStatus(QStringLiteral("%1 Notizen: %2")
                            .arg(m_notes.size())
                            .arg(m_notes.join(QStringLiteral(", "))));
      speak(intent.spokenReply);
      break;
    case BlopAssistantAction::OpenUrl:
      if (!QDesktopServices::openUrl(intent.url))
        m_pill->setStatus(QStringLiteral("Browser ließ sich nicht öffnen."));
      else {
        m_pill->setStatus(QStringLiteral("Browser: %1").arg(intent.url.host()));
        speak(intent.spokenReply);
      }
      break;
    case BlopAssistantAction::LaunchApp:
      if (!launchApp(intent.appName))
        m_pill->setStatus(
            QStringLiteral("App „%1“ nicht gefunden.").arg(intent.appName));
      else {
        m_pill->setStatus(QStringLiteral("Gestartet: %1").arg(intent.appName));
        speak(intent.spokenReply);
      }
      break;
    case BlopAssistantAction::Help:
      m_pill->setStatus(BlopAssistantEngine::helpText());
      speak(QStringLiteral(
          "Ich kann Notizen anlegen, den Browser öffnen und Apps starten."));
      break;
    case BlopAssistantAction::Unknown:
    default:
      m_pill->setStatus(intent.statusLine);
      break;
    }
  }

  BlopAssistantOverlay *m_pill{nullptr};
  QStringList m_notes;
  BlopAssistantIntent m_pending;
};

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("BlopAssistantSandbox"));
  app.setOrganizationName(QStringLiteral("Blop"));
  app.setQuitOnLastWindowClosed(false);

  const QStringList args = app.arguments();
  if (args.contains(QStringLiteral("--self-test")))
    return runSelfTest();

  auto *pill = new BlopAssistantOverlay(nullptr);
  pill->setHeadline(QStringLiteral("Blop  ·  Voice companion"));
  pill->setStandalone(true);

  auto *sc = new QShortcut(
      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space), pill);
  sc->setContext(Qt::ApplicationShortcut);
  QObject::connect(sc, &QShortcut::activated, pill,
                   &BlopAssistantOverlay::toggleExpanded);
  auto *scA =
      new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A), pill);
  scA->setContext(Qt::ApplicationShortcut);
  QObject::connect(scA, &QShortcut::activated, pill,
                   &BlopAssistantOverlay::toggleExpanded);

  auto *companion = new Companion(pill, &app);
  Q_UNUSED(companion);

#ifdef Q_OS_WIN
  auto *hotkeys = new WinHotkeyFilter(pill);
  app.installNativeEventFilter(hotkeys);
  QTimer::singleShot(0, pill, [pill]() {
    RegisterHotKey(reinterpret_cast<HWND>(pill->winId()), 1,
                   MOD_CONTROL | MOD_SHIFT, VK_SPACE);
  });
#endif

  return app.exec();
}
