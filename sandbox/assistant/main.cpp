#include "blopassistantengine.h"
#include "blopassistantoverlay.h"
#include "notechrome.h"
#include "uiscale.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QShowEvent>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdio>

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

int runSelfTest() {
  int fails = 0;
  auto expect = [&](const QString &utterance, BlopAssistantAction action,
                    const QString &titleOrQuery) {
    const BlopAssistantIntent i = BlopAssistantEngine::parse(utterance);
    if (i.action != action) {
      QTextStream(stderr) << "FAIL action: [" << utterance << "] got "
                          << int(i.action) << " expected " << int(action)
                          << '\n';
      ++fails;
      return;
    }
    const QString got =
        (action == BlopAssistantAction::CreateNote) ? i.title : i.query;
    if (!titleOrQuery.isEmpty() &&
        got.compare(titleOrQuery, Qt::CaseInsensitive) != 0) {
      QTextStream(stderr) << "FAIL payload: [" << utterance << "] got [" << got
                          << "] expected [" << titleOrQuery << "]\n";
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

  const BlopAssistantIntent inf =
      BlopAssistantEngine::parse(QStringLiteral("unendliche Notiz Skizze"));
  if (!inf.infinite) {
    QTextStream(stderr) << "FAIL infinite flag\n";
    ++fails;
  }

  if (fails == 0)
    QTextStream(stdout) << "blop-assistant-sandbox self-test: ok\n";
  else
    QTextStream(stderr) << "blop-assistant-sandbox self-test: " << fails
                        << " failed\n";
  return fails == 0 ? 0 : 1;
}

} // namespace

class AssistantSandboxWindow : public QMainWindow {
public:
  explicit AssistantSandboxWindow(QWidget *parent = nullptr)
      : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Blop Assistant — Sandbox (nicht Blop)"));
    resize(960, 640);
    setMinimumSize(720, 480);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 20, 20, 24);
    root->setSpacing(14);

    auto *banner = new QLabel(
        QStringLiteral("Eigenständiger Test — nicht in Blop eingebunden.\n"
                       "Hier kannst du Befehle ausprobieren, bevor der "
                       "Assistent in die App kommt."),
        central);
    banner->setWordWrap(true);
    banner->setObjectName(QStringLiteral("SandboxBanner"));
    root->addWidget(banner);

    auto *hint = new QLabel(
        QStringLiteral("Shortcut: Ctrl+Shift+Leertaste oder Ctrl+Shift+A"),
        central);
    hint->setObjectName(QStringLiteral("SandboxHint"));
    root->addWidget(hint);

    auto *cols = new QHBoxLayout();
    cols->setSpacing(14);

    auto *left = new QVBoxLayout();
    auto *notesLabel = new QLabel(QStringLiteral("Simulierte Notizen"), central);
    notesLabel->setObjectName(QStringLiteral("SandboxSection"));
    m_notes = new QListWidget(central);
    m_notes->setObjectName(QStringLiteral("SandboxNotes"));
    left->addWidget(notesLabel);
    left->addWidget(m_notes, 1);

    auto *right = new QVBoxLayout();
    auto *logLabel = new QLabel(QStringLiteral("Was der Assistent tun würde"),
                                central);
    logLabel->setObjectName(QStringLiteral("SandboxSection"));
    m_log = new QPlainTextEdit(central);
    m_log->setReadOnly(true);
    m_log->setObjectName(QStringLiteral("SandboxLog"));
    right->addWidget(logLabel);
    right->addWidget(m_log, 1);

    cols->addLayout(left, 1);
    cols->addLayout(right, 1);
    root->addLayout(cols, 1);

    auto *row = new QHBoxLayout();
    auto *showBtn = new QPushButton(QStringLiteral("Assistent öffnen"), central);
    showBtn->setObjectName(QStringLiteral("SandboxOpenBtn"));
    showBtn->setCursor(Qt::PointingHandCursor);
    auto *clearBtn = new QPushButton(QStringLiteral("Log leeren"), central);
    clearBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(showBtn);
    row->addWidget(clearBtn);
    row->addStretch(1);
    root->addLayout(row);

    applyTheme(central);

    for (const QString &n : loadNotes())
      m_notes->addItem(n);
    if (m_notes->count() == 0) {
      m_notes->addItem(QStringLiteral("Willkommen (Beispiel)"));
      persistNotes();
    }

    m_assistant = new BlopAssistantOverlay(this);
    m_assistant->setHeadline(QStringLiteral("Blop Assistant · Sandbox"));
    connect(m_assistant, &BlopAssistantOverlay::utteranceSubmitted, this,
            &AssistantSandboxWindow::onUtterance);
    connect(showBtn, &QPushButton::clicked, this, [this]() {
      m_assistant->setExpanded(true);
    });
    connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

    auto bind = [this](const QKeySequence &seq) {
      auto *sc = new QShortcut(seq, this);
      sc->setContext(Qt::WindowShortcut);
      connect(sc, &QShortcut::activated, this,
              [this]() { m_assistant->toggleExpanded(); });
    };
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
    bind(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));

    logLine(QStringLiteral("Sandbox bereit. Dateien unter:\n%1")
                .arg(sandboxDir()));
    logLine(BlopAssistantEngine::helpText());
  }

protected:
  void showEvent(QShowEvent *event) override {
    QMainWindow::showEvent(event);
    if (m_assistant && !m_assistant->isExpanded())
      m_assistant->setExpanded(true);
    if (m_assistant)
      m_assistant->reposition();
  }

  void resizeEvent(QResizeEvent *event) override {
    QMainWindow::resizeEvent(event);
    if (m_assistant && m_assistant->isExpanded())
      m_assistant->reposition();
  }

private:
  void applyTheme(QWidget *central) {
    const QString bg = NoteChrome::canvasBg().name(QColor::HexRgb);
    const QString panel = NoteChrome::panelElevated().name(QColor::HexRgb);
    const QString border = NoteChrome::border().name(QColor::HexRgb);
    const QString text = NoteChrome::textPrimary().name(QColor::HexRgb);
    const QString muted = NoteChrome::textSecondary().name(QColor::HexRgb);
    const QString accent = NoteChrome::accent().name(QColor::HexRgb);
    central->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; color: %2; }"
        "QLabel#SandboxBanner { color: %2; font-size: 15px; font-weight: 600; }"
        "QLabel#SandboxHint { color: %3; font-size: 12px; }"
        "QLabel#SandboxSection { color: %3; font-weight: 700; font-size: 11px; }"
        "QListWidget#SandboxNotes, QPlainTextEdit#SandboxLog {"
        "  background: %4; color: %2; border: 1px solid %5; border-radius: 12px;"
        "  padding: 8px; font-size: 13px;"
        "}"
        "QPushButton {"
        "  background: %6; color: #0B1220; border: none; border-radius: 12px;"
        "  padding: 10px 16px; font-weight: 700;"
        "}"
        "QPushButton:hover { background: #6AA8FF; }")
                               .arg(bg, text, muted, panel, border, accent));
  }

  void logLine(const QString &line) {
    if (m_log)
      m_log->appendPlainText(line);
  }

  void persistNotes() {
    QStringList names;
    for (int i = 0; i < m_notes->count(); ++i)
      names << m_notes->item(i)->text();
    saveNotes(names);
  }

  QString uniqueName(const QString &title) const {
    QString base = title.trimmed();
    if (base.isEmpty())
      base = QStringLiteral("Neue Notiz");
    QString name = base;
    int n = 2;
    auto exists = [&](const QString &candidate) {
      for (int i = 0; i < m_notes->count(); ++i) {
        if (m_notes->item(i)->text().compare(candidate, Qt::CaseInsensitive) ==
            0)
          return true;
      }
      return false;
    };
    while (exists(name))
      name = base + QStringLiteral(" (%1)").arg(n++);
    return name;
  }

  bool openMatching(const QString &query) {
    const QString q = query.trimmed();
    int best = -1;
    int bestScore = 0;
    for (int i = 0; i < m_notes->count(); ++i) {
      const QString name = m_notes->item(i)->text();
      const QString n = name.toLower();
      const QString qq = q.toLower();
      int score = 0;
      if (n == qq)
        score = 300;
      else if (n.startsWith(qq))
        score = 200;
      else if (n.contains(qq))
        score = 100;
      if (score > bestScore) {
        bestScore = score;
        best = i;
      }
    }
    if (best < 0)
      return false;
    m_notes->setCurrentRow(best);
    m_notes->scrollToItem(m_notes->item(best));
    return true;
  }

  void onUtterance(const QString &text) {
    const BlopAssistantIntent intent = BlopAssistantEngine::parse(text);
    m_assistant->setStatus(intent.statusLine);

    switch (intent.action) {
    case BlopAssistantAction::CreateNote: {
      const QString name = uniqueName(intent.title);
      const QString kind =
          intent.infinite ? QStringLiteral("unendlich / Leinwand")
                          : QStringLiteral("A4-Notiz");
      m_notes->addItem(name);
      m_notes->setCurrentRow(m_notes->count() - 1);
      persistNotes();
      QFile body(sandboxDir() + QLatin1Char('/') + name +
                 QStringLiteral(".txt"));
      if (body.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&body);
        out << name << '\n'
            << (intent.infinite ? "format: infinite\n" : "format: a4\n");
        if (!intent.body.isEmpty())
          out << intent.body << '\n';
      }
      m_assistant->setStatus(QStringLiteral("Sandbox: „%1“ angelegt (%2).")
                                 .arg(name, kind));
      logLine(QStringLiteral("CREATE  %1  [%2]%3")
                  .arg(name, kind,
                       intent.body.isEmpty()
                           ? QString()
                           : QStringLiteral("\n        text: %1").arg(intent.body)));
      break;
    }
    case BlopAssistantAction::OpenNote:
      if (openMatching(intent.query)) {
        m_assistant->setStatus(QStringLiteral("Sandbox: „%1“ ausgewählt.")
                                   .arg(m_notes->currentItem()->text()));
        logLine(QStringLiteral("OPEN    %1").arg(m_notes->currentItem()->text()));
      } else {
        m_assistant->setStatus(
            QStringLiteral("Sandbox: keine Notiz „%1“ — filtere die Liste.")
                .arg(intent.query));
        filterNotes(intent.query);
        logLine(QStringLiteral("OPEN?   %1 (nicht gefunden, Suche)").arg(intent.query));
      }
      break;
    case BlopAssistantAction::SearchNotes:
      filterNotes(intent.query);
      m_assistant->setStatus(
          QStringLiteral("Sandbox: Liste gefiltert nach „%1“.").arg(intent.query));
      logLine(QStringLiteral("SEARCH  %1").arg(intent.query));
      break;
    case BlopAssistantAction::ShowLibrary:
      filterNotes(QString());
      m_assistant->setStatus(QStringLiteral("Sandbox: ganze Liste."));
      logLine(QStringLiteral("LIBRARY"));
      break;
    case BlopAssistantAction::Help:
      m_assistant->setStatus(BlopAssistantEngine::helpText());
      logLine(QStringLiteral("HELP"));
      break;
    case BlopAssistantAction::Unknown:
    default:
      m_assistant->setStatus(intent.statusLine +
                             QStringLiteral("  Tipp: „neue Notiz Einkauf“."));
      logLine(QStringLiteral("UNKNOWN %1").arg(text));
      break;
    }
  }

  void filterNotes(const QString &query) {
    const QString q = query.trimmed().toLower();
    for (int i = 0; i < m_notes->count(); ++i) {
      const bool vis =
          q.isEmpty() || m_notes->item(i)->text().toLower().contains(q);
      m_notes->item(i)->setHidden(!vis);
    }
  }

  QListWidget *m_notes{nullptr};
  QPlainTextEdit *m_log{nullptr};
  BlopAssistantOverlay *m_assistant{nullptr};
};

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("BlopAssistantSandbox"));
  app.setOrganizationName(QStringLiteral("Blop"));

  const QStringList args = app.arguments();
  if (args.contains(QStringLiteral("--self-test")))
    return runSelfTest();

  AssistantSandboxWindow w;
  w.show();
  return app.exec();
}
