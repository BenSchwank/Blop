#include "blopassistantengine.h"
#include "blopassistantllm.h"
#include "blopassistantmic.h"
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
#include <QGuiApplication>
#include <QScreen>
#include <QJsonObject>
#include <QByteArray>
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

  const BlopAssistantIntent hi =
      BlopAssistantEngine::parse(QStringLiteral("hallo"));
  if (hi.action != BlopAssistantAction::Talk) {
    QTextStream(stderr) << "FAIL hallo should be talk, not a note\n";
    ++fails;
  }

  BlopAssistantIntent toolIntent;
  QJsonObject noteArgs;
  noteArgs.insert(QStringLiteral("title"), QStringLiteral("Einkauf"));
  if (!BlopAssistantLlm::intentFromTool(QStringLiteral("create_note"), noteArgs,
                                        &toolIntent) ||
      toolIntent.action != BlopAssistantAction::CreateNote ||
      toolIntent.title != QStringLiteral("Einkauf")) {
    QTextStream(stderr) << "FAIL llm create_note tool\n";
    ++fails;
  }

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

  const QByteArray wav = BlopAssistantMic::makeWav(QByteArray(3200, char(0)), 16000);
  if (!wav.startsWith("RIFF") || wav.size() != 44 + 3200) {
    QTextStream(stderr) << "FAIL wav header\n";
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
  else if (key == Qt::Key_Return || key == Qt::Key_Enter)
    v = VK_RETURN;
  else if (key == Qt::Key_Tab)
    return false;
  else {
    const SHORT scan = VkKeyScanW(ushort(key));
    if (scan == -1)
      return false;
    v = LOBYTE(scan);
    if (v == 0)
      return false;
  }
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
    connect(notch, &BlopAssistantOverlay::audioCaptured, this,
            &Companion::onAudio);
    connect(notch, &BlopAssistantOverlay::confirmAccepted, this,
            &Companion::onConfirm);
    connect(notch, &BlopAssistantOverlay::confirmRejected, this, [this]() {
      m_pending = {};
      m_pending.action = BlopAssistantAction::Unknown;
    });

    m_settings = new BlopAssistantSettingsDialog();
    connect(m_settings, &BlopAssistantSettingsDialog::prefsChanged, this,
            [this]() {
              if (m_settings && m_settings->isVisible()) {
#ifdef Q_OS_WIN
                unregisterWinHotkeys();
#endif
                return;
              }
              rebindShortcuts();
              refreshEmpty();
            });

    m_llm = new BlopAssistantLlm(this);
    m_llm->setToolHandler([this](const BlopAssistantIntent &intent) {
      if (intent.needsConfirm) {
        m_pending = intent;
        m_notch->promptConfirm(intent.statusLine);
        return QStringLiteral("warte auf bestätigung vom nutzer");
      }
      return executeIntent(intent, false);
    });
    connect(m_llm, &BlopAssistantLlm::thinking, this,
            [this]() { m_notch->setBusy(true); });
    connect(m_llm, &BlopAssistantLlm::assistantSaid, this,
            [this](const QString &text) {
              m_notch->setBusy(false);
              reply(text, text);
            });
    connect(m_llm, &BlopAssistantLlm::failed, this, [this](const QString &err) {
      m_notch->setBusy(false);
      reply(err, QStringLiteral("Das hat nicht geklappt."));
    });
    refreshEmpty();

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
    menu->addAction(QStringLiteral("Zuhören"), notch,
                    &BlopAssistantOverlay::beginListen);
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
    hotkeys->toggleChat = [this]() { startHold(1); };
    hotkeys->pushToTalkDown = [this]() { startHold(2); };
    qApp->installNativeEventFilter(hotkeys);
    QTimer::singleShot(0, this, [this]() { registerWinHotkeys(); });
    m_pttPoll = new QTimer(this);
    m_pttPoll->setInterval(40);
    connect(m_pttPoll, &QTimer::timeout, this, [this]() {
      if (m_heldVk == 0)
        return;
      if (!(GetAsyncKeyState(static_cast<int>(m_heldVk)) & 0x8000)) {
        m_pttPoll->stop();
        m_heldVk = 0;
        m_notch->endPushToTalk();
      }
    });
#endif
  }

  bool eventFilter(QObject *obj, QEvent *event) override {
    Q_UNUSED(obj);
    if (m_settings && m_settings->isVisible())
      return false;
    if (qobject_cast<QKeySequenceEdit *>(QApplication::focusWidget()))
      return false;
    if (event->type() == QEvent::KeyPress) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (!ke->isAutoRepeat() &&
          (sequenceMatches(BlopAssistantPrefs::openChatSequence(), ke) ||
           sequenceMatches(BlopAssistantPrefs::pushToTalkSequence(), ke))) {
        m_notch->startPushToTalk();
        return false;
      }
    }
    if (event->type() == QEvent::KeyRelease) {
      auto *ke = static_cast<QKeyEvent *>(event);
      if (!ke->isAutoRepeat() &&
          (sequenceMatches(BlopAssistantPrefs::openChatSequence(), ke) ||
           sequenceMatches(BlopAssistantPrefs::pushToTalkSequence(), ke))) {
        m_notch->endPushToTalk();
        return false;
      }
    }
    return false;
  }

private:
  void showSettings() {
    m_notch->setExpanded(false);
#ifdef Q_OS_WIN
    unregisterWinHotkeys();
#endif
    m_settings->show();
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
      const QRect geo = screen->availableGeometry();
      m_settings->adjustSize();
      const QSize sz = m_settings->frameGeometry().size();
      m_settings->move(geo.x() + (geo.width() - sz.width()) / 2,
                       geo.y() + (geo.height() - sz.height()) / 2);
    }
    m_settings->raise();
    m_settings->activateWindow();
    connect(m_settings, &QDialog::finished, this, [this]() {
      rebindShortcuts();
      refreshEmpty();
    }, Qt::UniqueConnection);
  }

  void rebindShortcuts() {
    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
#ifdef Q_OS_WIN
    registerWinHotkeys();
#endif
  }

#ifdef Q_OS_WIN
  void unregisterWinHotkeys() {
    if (!m_notch)
      return;
    const HWND hwnd = reinterpret_cast<HWND>(m_notch->winId());
    UnregisterHotKey(hwnd, 1);
    UnregisterHotKey(hwnd, 2);
    m_wakeVk = 0;
    m_pttVk = 0;
    m_heldVk = 0;
    if (m_pttPoll)
      m_pttPoll->stop();
  }

  void startHold(int which) {
    const UINT vk = (which == 1) ? m_wakeVk : m_pttVk;
    if (vk == 0)
      return;
    m_heldVk = vk;
    m_notch->startPushToTalk();
    if (m_pttPoll)
      m_pttPoll->start();
  }

  void registerWinHotkeys() {
    if (!m_notch)
      return;
    unregisterWinHotkeys();
    UINT mods = 0, vk = 0;
    if (sequenceToWin(BlopAssistantPrefs::openChatSequence(), &mods, &vk)) {
      m_wakeVk = vk;
      RegisterHotKey(reinterpret_cast<HWND>(m_notch->winId()), 1,
                     mods | MOD_NOREPEAT, vk);
    }
    mods = 0;
    vk = 0;
    if (sequenceToWin(BlopAssistantPrefs::pushToTalkSequence(), &mods, &vk)) {
      m_pttVk = vk;
      RegisterHotKey(reinterpret_cast<HWND>(m_notch->winId()), 2,
                     mods | MOD_NOREPEAT, vk);
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

  void refreshEmpty() {
    if (BlopAssistantLlm::isReady())
      m_notch->setEmptyHint(
          QStringLiteral("Frag mich irgendwas — ich antworte wie ein Mensch."));
    else
      m_notch->setEmptyHint(
          QStringLiteral("Ohne API-Key nur Smalltalk und Befehle.\n"
                         "Key unter Einstellungen → KI, dann bin ich frei."));
  }

  void reply(const QString &text, const QString &spoken) {
    m_notch->addAssistantMessage(text);
    BlopAssistantVoice::speak(spoken);
  }

  void onAudio(const QByteArray &wav) {
    m_notch->setListenCaption(QStringLiteral("Erkenne Sprache…"));
    m_llm->transcribe(wav, [this](const QString &text, const QString &error) {
      if (!error.isEmpty() || text.trimmed().isEmpty()) {
        const QString msg =
            error.isEmpty()
                ? QStringLiteral("Nichts verstanden.")
                : error;
        m_notch->setListenCaption(msg);
        QTimer::singleShot(2400, this, [this]() {
          m_notch->setListenCaption(QString());
          m_notch->clearListenBusy();
        });
        return;
      }
      m_notch->acceptTranscript(text);
    });
  }

  void onUtterance(const QString &text) {
    if (BlopAssistantLlm::isReady()) {
      m_notch->setBusy(true);
      m_llm->ask(text, m_notes);
      return;
    }
    BlopAssistantIntent intent = BlopAssistantEngine::parse(text);
    if (intent.action == BlopAssistantAction::Unknown) {
      intent = BlopAssistantEngine::talk(QStringLiteral(
          "Dazu würde ich dir gerne frei antworten. Trag unter "
          "Einstellungen → KI einen API-Key ein — OpenAI, Groq oder "
          "OpenRouter reicht. Bis dahin: Notizen, Browser, Apps."));
    }
    if (intent.needsConfirm) {
      m_pending = intent;
      m_notch->promptConfirm(intent.statusLine);
      return;
    }
    executeIntent(intent, true);
  }

  void onConfirm() {
    if (m_pending.action == BlopAssistantAction::Unknown)
      return;
    executeIntent(m_pending, true);
    m_pending = {};
    m_pending.action = BlopAssistantAction::Unknown;
  }

  QString executeIntent(const BlopAssistantIntent &intent, bool announce) {
    QString result;
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
      result = QStringLiteral("notiz angelegt: %1").arg(name);
      if (announce)
        reply(QStringLiteral("Alles klar, „%1“ liegt in den Notizen.")
                  .arg(name),
              intent.spokenReply);
      break;
    }
    case BlopAssistantAction::OpenNote: {
      QString opened;
      if (openMatching(intent.query, &opened)) {
        result = QStringLiteral("gefunden: %1").arg(opened);
        if (announce)
          reply(QStringLiteral("Die liegt hier: „%1“.").arg(opened),
                intent.spokenReply);
      } else {
        result = QStringLiteral("nicht gefunden");
        if (announce)
          reply(QStringLiteral("Die Notiz „%1“ hab ich nicht.")
                    .arg(intent.query),
                QStringLiteral("Die Notiz hab ich nicht gefunden."));
      }
      break;
    }
    case BlopAssistantAction::SearchNotes: {
      int hits = 0;
      for (const QString &n : m_notes) {
        if (n.contains(intent.query, Qt::CaseInsensitive))
          ++hits;
      }
      result = QStringLiteral("%1 treffer").arg(hits);
      if (announce)
        reply(QStringLiteral("%1 Treffer für „%2“.")
                  .arg(hits)
                  .arg(intent.query),
              intent.spokenReply);
      break;
    }
    case BlopAssistantAction::ShowLibrary:
      result = m_notes.join(QStringLiteral(", "));
      if (announce)
        reply(QStringLiteral("%1 Notizen: %2")
                  .arg(m_notes.size())
                  .arg(m_notes.join(QStringLiteral(", "))),
              intent.spokenReply);
      break;
    case BlopAssistantAction::OpenUrl:
      if (!QDesktopServices::openUrl(intent.url)) {
        result = QStringLiteral("browser fehlgeschlagen");
        if (announce)
          reply(QStringLiteral("Der Browser will gerade nicht."),
                QStringLiteral("Der Browser geht gerade nicht."));
      } else {
        result = QStringLiteral("browser geöffnet: %1").arg(intent.url.host());
        if (announce)
          reply(QStringLiteral("Ist offen: %1.").arg(intent.url.host()),
                intent.spokenReply);
      }
      break;
    case BlopAssistantAction::LaunchApp:
      if (!launchApp(intent.appName)) {
        result = QStringLiteral("app nicht gefunden");
        if (announce)
          reply(QStringLiteral("„%1“ hab ich nicht gefunden.")
                    .arg(intent.appName),
                QStringLiteral("Die App hab ich nicht gefunden."));
      } else {
        result = QStringLiteral("gestartet: %1").arg(intent.appName);
        if (announce)
          reply(QStringLiteral("Läuft: %1.").arg(intent.appName),
                intent.spokenReply);
      }
      break;
    case BlopAssistantAction::Help:
      result = QStringLiteral("hilfe");
      if (announce)
        reply(BlopAssistantEngine::helpText(), intent.spokenReply);
      break;
    case BlopAssistantAction::Talk:
      result = QStringLiteral("talk");
      if (announce)
        reply(intent.statusLine, intent.spokenReply);
      break;
    case BlopAssistantAction::Unknown:
    default:
      result = QStringLiteral("unbekannt");
      if (announce)
        reply(intent.statusLine, intent.spokenReply);
      break;
    }
    return result;
  }

  BlopAssistantOverlay *m_notch{nullptr};
  BlopAssistantSettingsDialog *m_settings{nullptr};
  BlopAssistantLlm *m_llm{nullptr};
  QList<QShortcut *> m_shortcuts;
  QStringList m_notes;
  BlopAssistantIntent m_pending;
#ifdef Q_OS_WIN
  QTimer *m_pttPoll{nullptr};
  UINT m_wakeVk{0};
  UINT m_pttVk{0};
  UINT m_heldVk{0};
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
