#include "blopassistantengine.h"
#include "blopassistantoverlay.h"
#include "blopassistantprefs.h"
#include "blopassistantsettingsdialog.h"
#include "blopassistantvoice.h"

#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QDesktopServices>
#include <QKeySequenceEdit>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QKeyCombination>
#include <QMenu>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <functional>
#include <QProcess>
#include <QShortcut>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

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

bool sequenceMatches(const QKeySequence &seq, const QKeyEvent *ke) {
  if (!ke || seq.isEmpty())
    return false;
  const QKeySequence pressed(QKeyCombination(
      ke->modifiers(), static_cast<Qt::Key>(ke->key())));
  return pressed.matches(seq) == QKeySequence::ExactMatch;
}

#ifdef Q_OS_WIN
bool sequenceToWin(const QKeySequence &seq, UINT *mods, UINT *vk) {
  if (seq.isEmpty() || seq.count() < 1)
    return false;
  const QKeyCombination combo = seq[0];
  const Qt::KeyboardModifiers km = combo.keyboardModifiers();
  const int key = combo.key();
  UINT m = 0;
  if (km & Qt::ControlModifier)
    m |= MOD_CONTROL;
  if (km & Qt::ShiftModifier)
    m |= MOD_SHIFT;
  if (km & Qt::AltModifier)
    m |= MOD_ALT;
  if (km & Qt::MetaModifier)
    m |= MOD_WIN;
  UINT v = 0;
  if (key == Qt::Key_Space)
    v = VK_SPACE;
  else if (key >= Qt::Key_A && key <= Qt::Key_Z)
    v = static_cast<UINT>(key);
  else if (key >= Qt::Key_0 && key <= Qt::Key_9)
    v = static_cast<UINT>(key);
  else if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
    v = static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
  else
    return false;
  *mods = m;
  *vk = v;
  return true;
}

class WinHotkeyFilter : public QAbstractNativeEventFilter {
public:
  std::function<void()> toggleChat;
  std::function<void()> pushToTalkDown;
  bool nativeEventFilter(const QByteArray &, void *message,
                         qintptr *) override {
    auto *msg = static_cast<MSG *>(message);
    if (!msg || msg->message != WM_HOTKEY)
      return false;
    if (msg->wParam == 1 && toggleChat)
      toggleChat();
    else if (msg->wParam == 2 && pushToTalkDown)
      pushToTalkDown();
    return false;
  }
};
#endif

class Companion : public QObject {
public:
  explicit Companion(BlopAssistantOverlay *notch, QObject *parent = nullptr)
      : QObject(parent), m_notch(notch) {
    m_notes = loadNotes();
    if (m_notes.isEmpty())
      m_notes << QStringLiteral("Willkommen (Beispiel)");
    saveNotes(m_notes);

    connect(notch, &BlopAssistantOverlay::utteranceSubmitted, this,
            &Companion::onUtterance);
    connect(notch, &BlopAssistantOverlay::confirmAccepted, this,
            &Companion::onConfirm);
    connect(notch, &BlopAssistantOverlay::confirmRejected, this, [this]() {
      m_pending = {};
      m_pending.action = BlopAssistantAction::Unknown;
    });

    m_settings = new BlopAssistantSettingsDialog();
    connect(m_settings, &BlopAssistantSettingsDialog::prefsChanged, this,
            &Companion::rebindShortcuts);

    auto *tray = new QSystemTrayIcon(this);
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(30, 30, 30));
    p.setPen(QPen(QColor(91, 157, 255), 3));
    p.drawEllipse(8, 8, 48, 48);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(91, 157, 255));
    p.drawRoundedRect(22, 30, 20, 4, 2, 2);
    p.end();
    tray->setIcon(QIcon(pm));
    tray->setToolTip(QStringLiteral("Blop Assistant — Einstellungen"));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("Chat öffnen"), notch, [notch]() {
      notch->setExpanded(true);
    });
    menu->addAction(QStringLiteral("Einstellungen"), this, [this]() {
      showSettings();
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Beenden"), qApp, &QApplication::quit);
    tray->setContextMenu(menu);
    connect(tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
              if (reason == QSystemTrayIcon::Trigger ||
                  reason == QSystemTrayIcon::DoubleClick)
                showSettings();
            });
    tray->show();

    rebindShortcuts();
    qApp->installEventFilter(this);

#ifdef Q_OS_WIN
    auto *hotkeys = new WinHotkeyFilter();
    hotkeys->toggleChat = [notch]() { notch->toggleExpanded(); };
    hotkeys->pushToTalkDown = [this]() {
      m_notch->startPushToTalk();
      QTimer::singleShot(180, this, [this]() {
        if (m_pttPoll)
          m_pttPoll->start();
      });
    };
    qApp->installNativeEventFilter(hotkeys);
    QTimer::singleShot(0, this, [this]() { registerWinHotkeys(); });
    m_pttPoll = new QTimer(this);
    m_pttPoll->setInterval(50);
    connect(m_pttPoll, &QTimer::timeout, this, [this]() {
      if (m_pttVk == 0)
        return;
      if (!(GetAsyncKeyState(static_cast<int>(m_pttVk)) & 0x8000)) {
        m_pttPoll->stop();
        m_notch->endPushToTalk();
      }
    });
#endif
  }

  bool eventFilter(QObject *obj, QEvent *event) override {
    Q_UNUSED(obj);
    if (qobject_cast<QKeySequenceEdit *>(QApplication::focusWidget()))
      return false;
    if (event->type() == QEvent::KeyPress) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (!ke->isAutoRepeat() &&
          sequenceMatches(BlopAssistantPrefs::pushToTalkSequence(), ke)) {
        m_notch->startPushToTalk();
        return false;
      }
    }
    if (event->type() == QEvent::KeyRelease) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (!ke->isAutoRepeat() &&
          sequenceMatches(BlopAssistantPrefs::pushToTalkSequence(), ke)) {
        m_notch->endPushToTalk();
        return false;
      }
    }
    return false;
  }

private:
  void showSettings() {
    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
  }

  void rebindShortcuts() {
    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
    auto add = [this](const QKeySequence &seq, auto slot) {
      if (seq.isEmpty())
        return;
      auto *sc = new QShortcut(seq, m_notch);
      sc->setContext(Qt::ApplicationShortcut);
      connect(sc, &QShortcut::activated, m_notch, slot);
      m_shortcuts << sc;
    };
    add(BlopAssistantPrefs::openChatSequence(),
        &BlopAssistantOverlay::toggleExpanded);
#ifdef Q_OS_WIN
    registerWinHotkeys();
#endif
  }

#ifdef Q_OS_WIN
  void registerWinHotkeys() {
    if (!m_notch)
      return;
    const HWND hwnd = reinterpret_cast<HWND>(m_notch->winId());
    UnregisterHotKey(hwnd, 1);
    UnregisterHotKey(hwnd, 2);
    UINT mods = 0, vk = 0;
    if (sequenceToWin(BlopAssistantPrefs::openChatSequence(), &mods, &vk))
      RegisterHotKey(hwnd, 1, mods | MOD_NOREPEAT, vk);
    mods = 0;
    vk = 0;
    if (sequenceToWin(BlopAssistantPrefs::pushToTalkSequence(), &mods, &vk)) {
      m_pttVk = vk;
      RegisterHotKey(hwnd, 2, mods | MOD_NOREPEAT, vk);
    } else {
      m_pttVk = 0;
    }
  }
#endif

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

  void reply(const QString &text, const QString &spoken) {
    m_notch->addAssistantMessage(text);
    BlopAssistantVoice::speak(spoken);
  }

  void onUtterance(const QString &text) {
    const BlopAssistantIntent intent = BlopAssistantEngine::parse(text);
    if (intent.needsConfirm) {
      m_pending = intent;
      m_notch->promptConfirm(intent.statusLine);
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
      reply(QStringLiteral("Notiz „%1“ ist drin.").arg(name),
            intent.spokenReply);
      break;
    }
    case BlopAssistantAction::OpenNote: {
      QString opened;
      if (openMatching(intent.query, &opened))
        reply(QStringLiteral("Ich hab „%1“ gefunden.").arg(opened),
              intent.spokenReply);
      else
        reply(QStringLiteral("Keine Notiz „%1“.").arg(intent.query),
              QStringLiteral("Die Notiz hab ich nicht gefunden."));
      break;
    }
    case BlopAssistantAction::SearchNotes: {
      int hits = 0;
      for (const QString &n : m_notes) {
        if (n.contains(intent.query, Qt::CaseInsensitive))
          ++hits;
      }
      reply(QStringLiteral("%1 Treffer für „%2“.").arg(hits).arg(intent.query),
            intent.spokenReply);
      break;
    }
    case BlopAssistantAction::ShowLibrary:
      reply(QStringLiteral("%1 Notizen: %2")
                .arg(m_notes.size())
                .arg(m_notes.join(QStringLiteral(", "))),
            intent.spokenReply);
      break;
    case BlopAssistantAction::OpenUrl:
      if (!QDesktopServices::openUrl(intent.url))
        reply(QStringLiteral("Browser ging nicht auf."),
              QStringLiteral("Der Browser geht gerade nicht."));
      else
        reply(QStringLiteral("Browser: %1").arg(intent.url.host()),
              intent.spokenReply);
      break;
    case BlopAssistantAction::LaunchApp:
      if (!launchApp(intent.appName))
        reply(QStringLiteral("„%1“ hab ich nicht gefunden.").arg(intent.appName),
              QStringLiteral("Die App hab ich nicht gefunden."));
      else
        reply(QStringLiteral("Gestartet: %1").arg(intent.appName),
              intent.spokenReply);
      break;
    case BlopAssistantAction::Help:
      reply(BlopAssistantEngine::helpText(), intent.spokenReply);
      break;
    case BlopAssistantAction::Unknown:
    default:
      reply(intent.statusLine, intent.spokenReply);
      break;
    }
  }

  BlopAssistantOverlay *m_notch{nullptr};
  BlopAssistantSettingsDialog *m_settings{nullptr};
  QList<QShortcut *> m_shortcuts;
  QStringList m_notes;
  BlopAssistantIntent m_pending;
#ifdef Q_OS_WIN
  QTimer *m_pttPoll{nullptr};
  UINT m_pttVk{0};
#endif
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

  auto *notch = new BlopAssistantOverlay(nullptr);
  notch->setHeadline(QStringLiteral("Blop"));
  notch->setStandalone(true);

  auto *companion = new Companion(notch, &app);
  Q_UNUSED(companion);

  return app.exec();
}
