#include "blop_diag.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(Q_OS_ANDROID) || defined(Q_OS_LINUX) || defined(Q_OS_DARWIN)
#  include <csignal>
#  include <fcntl.h>
#  include <sys/types.h>
#  include <unistd.h>
#  define BLOP_DIAG_HAS_POSIX 1
#else
#  define BLOP_DIAG_HAS_POSIX 0
#endif

#if defined(Q_OS_ANDROID) || defined(Q_OS_LINUX)
#  include <unwind.h>
#  include <dlfcn.h>
#  define BLOP_DIAG_HAS_UNWIND 1
#else
#  define BLOP_DIAG_HAS_UNWIND 0
#endif

namespace BlopDiag {

namespace {

// ---------------------------------------------------------------------------
// Ring buffer: 256 lines, each up to 240 bytes. POD-only so it can be
// touched from a signal handler without UB.
// ---------------------------------------------------------------------------
constexpr int kRingSlots = 256;
constexpr int kRingLineBytes = 240;

struct RingSlot {
  char text[kRingLineBytes];
  // 0 = empty, 1 = filled (only relevant on first wrap-around).
  std::atomic<int> filled{0};
};

RingSlot g_ring[kRingSlots];
std::atomic<int> g_ringIndex{0};

QtMessageHandler g_chainedQtHandler = nullptr;
std::atomic<bool> g_installed{false};

// Pre-resolved crash file path (UTF-8). Reserved buffer up to 1024 bytes.
char g_crashPath[1024] = {0};

// Pre-opened crash file descriptor; -1 if open() failed. Best-effort: we
// rotate a fresh fd on every install() call so a previous crash file is
// overwritten on next start (after replay).
int g_crashFd = -1;

void writeLineToRing(const char *prefix, const char *body) {
  if (!body) {
    return;
  }
  const int slot = g_ringIndex.fetch_add(1, std::memory_order_relaxed) % kRingSlots;
  RingSlot &s = g_ring[slot];

  // Build "<elapsed_ms> <prefix> <body>" without allocating.
  char *p = s.text;
  char *end = s.text + kRingLineBytes - 1;

  static const auto t0 = QDateTime::currentMSecsSinceEpoch();
  qint64 ms = QDateTime::currentMSecsSinceEpoch() - t0;
  if (ms < 0) ms = 0;
  // Manual itoa (no malloc, no std::string).
  char num[24];
  int ni = 0;
  if (ms == 0) {
    num[ni++] = '0';
  } else {
    while (ms > 0 && ni < (int)sizeof(num)) {
      num[ni++] = char('0' + (ms % 10));
      ms /= 10;
    }
  }
  while (ni > 0 && p < end) {
    *p++ = num[--ni];
  }
  if (p < end) *p++ = ' ';

  if (prefix) {
    while (*prefix && p < end) *p++ = *prefix++;
    if (p < end) *p++ = ' ';
  }
  if (body) {
    while (*body && p < end) *p++ = *body++;
  }
  *p = '\0';
  s.filled.store(1, std::memory_order_release);
}

// Dump the ring buffer in chronological order (oldest -> newest).
// Forward declarations to allow Win/non-POSIX branch to skip definitions.

#if BLOP_DIAG_HAS_POSIX
// Async-signal-safe writes only.
ssize_t safeWriteAll(int fd, const void *data, size_t n) {
  if (fd < 0) return -1;
  const char *p = static_cast<const char *>(data);
  size_t left = n;
  while (left > 0) {
    ssize_t w = ::write(fd, p, left);
    if (w < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (w == 0) break;
    p += w;
    left -= (size_t)w;
  }
  return (ssize_t)(n - left);
}

void safeWriteCStr(int fd, const char *s) {
  if (!s || fd < 0) return;
  size_t n = 0;
  while (s[n] != '\0' && n < (1u << 20)) ++n;
  safeWriteAll(fd, s, n);
}

void safeWriteHex(int fd, uintptr_t v) {
  char buf[2 + 16 + 1];
  buf[0] = '0';
  buf[1] = 'x';
  static const char *hex = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    buf[2 + (15 - i)] = hex[(v >> (i * 4)) & 0xF];
  }
  buf[2 + 16] = '\0';
  safeWriteCStr(fd, buf);
}

void safeWriteInt(int fd, long v) {
  char buf[32];
  int i = 0;
  if (v < 0) {
    safeWriteCStr(fd, "-");
    v = -v;
  }
  if (v == 0) {
    buf[i++] = '0';
  } else {
    while (v > 0 && i < (int)sizeof(buf)) {
      buf[i++] = char('0' + (v % 10));
      v /= 10;
    }
  }
  while (i-- > 0) {
    char c = buf[i];
    safeWriteAll(fd, &c, 1);
  }
}

void safeDumpRing(int fd) {
  const int idx = g_ringIndex.load(std::memory_order_relaxed);
  // The slot for the next write is idx % kRingSlots; that's the oldest.
  int start = idx % kRingSlots;
  for (int i = 0; i < kRingSlots; ++i) {
    int s = (start + i) % kRingSlots;
    if (g_ring[s].filled.load(std::memory_order_acquire) == 0) continue;
    safeWriteCStr(fd, g_ring[s].text);
    safeWriteCStr(fd, "\n");
  }
}
#endif // BLOP_DIAG_HAS_POSIX

#if BLOP_DIAG_HAS_UNWIND
struct UnwindCtx {
  int fd;
  int frame;
  int max;
};

_Unwind_Reason_Code unwindCb(struct _Unwind_Context *ctx, void *arg) {
  UnwindCtx *u = static_cast<UnwindCtx *>(arg);
  if (u->frame >= u->max) return _URC_END_OF_STACK;
  uintptr_t pc = _Unwind_GetIP(ctx);
  if (pc == 0) return _URC_END_OF_STACK;

  safeWriteCStr(u->fd, "  #");
  safeWriteInt(u->fd, u->frame);
  safeWriteCStr(u->fd, " pc ");
  safeWriteHex(u->fd, pc);

  Dl_info info;
  if (dladdr(reinterpret_cast<void *>(pc), &info) && info.dli_fname) {
    safeWriteCStr(u->fd, " ");
    safeWriteCStr(u->fd, info.dli_fname);
    if (info.dli_sname) {
      safeWriteCStr(u->fd, " (");
      safeWriteCStr(u->fd, info.dli_sname);
      safeWriteCStr(u->fd, ")");
    }
  }
  safeWriteCStr(u->fd, "\n");
  ++u->frame;
  return _URC_NO_REASON;
}
#endif // BLOP_DIAG_HAS_UNWIND

#if BLOP_DIAG_HAS_POSIX
const char *signalName(int sig) {
  switch (sig) {
  case SIGSEGV:
    return "SIGSEGV";
  case SIGABRT:
    return "SIGABRT";
  case SIGBUS:
    return "SIGBUS";
  case SIGILL:
    return "SIGILL";
  case SIGFPE:
    return "SIGFPE";
  default:
    return "SIG?";
  }
}

void crashSignalHandler(int sig, siginfo_t *info, void *uctx) {
  Q_UNUSED(uctx);
  // Async-signal-safe block. NO malloc, NO QString, NO QFile.
  const int fd = g_crashFd;
  if (fd >= 0) {
    safeWriteCStr(fd, "==== BLOP CRASH ====\n");
    safeWriteCStr(fd, "signal: ");
    safeWriteCStr(fd, signalName(sig));
    safeWriteCStr(fd, " (");
    safeWriteInt(fd, sig);
    safeWriteCStr(fd, ")\n");
    if (info) {
      safeWriteCStr(fd, "fault_addr: ");
      safeWriteHex(fd, reinterpret_cast<uintptr_t>(info->si_addr));
      safeWriteCStr(fd, "\n");
      safeWriteCStr(fd, "si_code: ");
      safeWriteInt(fd, info->si_code);
      safeWriteCStr(fd, "\n");
    }
    safeWriteCStr(fd, "pid: ");
    safeWriteInt(fd, (long)::getpid());
    safeWriteCStr(fd, "\n");
    safeWriteCStr(fd, "---- backtrace ----\n");
#if BLOP_DIAG_HAS_UNWIND
    UnwindCtx u{fd, 0, 64};
    _Unwind_Backtrace(unwindCb, &u);
#else
    safeWriteCStr(fd, "(unwind unavailable on this platform)\n");
#endif
    safeWriteCStr(fd, "---- recent log + ui actions ----\n");
    safeDumpRing(fd);
    safeWriteCStr(fd, "==== END ====\n");
    ::fsync(fd);
    ::close(fd);
    g_crashFd = -1;
  }

  // Re-raise default to let the OS / Android tombstone subsystem proceed.
  struct sigaction sa{};
  sa.sa_handler = SIG_DFL;
  ::sigemptyset(&sa.sa_mask);
  ::sigaction(sig, &sa, nullptr);
  ::raise(sig);
}

void installSignalHandlers() {
  struct sigaction sa{};
  sa.sa_sigaction = &crashSignalHandler;
  sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
  ::sigemptyset(&sa.sa_mask);
  // Mask peer signals during handler so we don't recurse.
  ::sigaddset(&sa.sa_mask, SIGSEGV);
  ::sigaddset(&sa.sa_mask, SIGABRT);
  ::sigaddset(&sa.sa_mask, SIGBUS);
  ::sigaddset(&sa.sa_mask, SIGILL);
  ::sigaddset(&sa.sa_mask, SIGFPE);

  ::sigaction(SIGSEGV, &sa, nullptr);
  ::sigaction(SIGABRT, &sa, nullptr);
  ::sigaction(SIGBUS, &sa, nullptr);
  ::sigaction(SIGILL, &sa, nullptr);
  ::sigaction(SIGFPE, &sa, nullptr);
}
#endif // BLOP_DIAG_HAS_POSIX

void qtMessageHandler(QtMsgType type, const QMessageLogContext &ctx,
                      const QString &msg) {
  const char *prefix = "?";
  switch (type) {
  case QtDebugMsg:
    prefix = "D";
    break;
  case QtInfoMsg:
    prefix = "I";
    break;
  case QtWarningMsg:
    prefix = "W";
    break;
  case QtCriticalMsg:
    prefix = "C";
    break;
  case QtFatalMsg:
    prefix = "F";
    break;
  }
  // Avoid allocating a long QString when ring-buffering. Just truncate.
  const QByteArray utf8 = msg.toUtf8();
  writeLineToRing(prefix, utf8.constData());

  // Forward to the previous handler so logcat / qDebug still work.
  if (g_chainedQtHandler && g_chainedQtHandler != &qtMessageHandler) {
    g_chainedQtHandler(type, ctx, msg);
  } else {
    // Fallback: ensure the line still hits stderr / logcat.
    std::fputs(prefix, stderr);
    std::fputs(": ", stderr);
    std::fputs(utf8.constData(), stderr);
    std::fputc('\n', stderr);
  }
}

QString crashFilePath() {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty()) {
    dir = QDir::tempPath();
  }
  QDir().mkpath(dir);
  return dir + QStringLiteral("/last_crash.txt");
}

QString previousCrashFilePath() {
  QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (dir.isEmpty()) {
    dir = QDir::tempPath();
  }
  return dir + QStringLiteral("/previous_crash.txt");
}

void rotatePreviousCrashFile() {
  // If last_crash.txt from the previous run contains an actual crash marker,
  // move it aside so install() can re-create a fresh fd without losing data.
  // takeCrashReportIfPresent() will pick up previous_crash.txt later.
  const QString cur = crashFilePath();
  const QString prev = previousCrashFilePath();
  QFile cf(cur);
  if (!cf.exists()) {
    return;
  }
  if (!cf.open(QIODevice::ReadOnly)) {
    return;
  }
  const QByteArray peek = cf.read(64 * 1024);
  cf.close();
  if (!peek.contains("==== BLOP CRASH ====")) {
    // Previous run exited cleanly -- the file just has the running-marker
    // header. We can drop it.
    QFile::remove(cur);
    return;
  }
  QFile::remove(prev);
  QFile::rename(cur, prev);
}

void openCrashFd() {
#if BLOP_DIAG_HAS_POSIX
  const QString path = crashFilePath();
  const QByteArray pathUtf8 = path.toUtf8();
  // Cache for crashReportPath() and the signal handler.
  std::strncpy(g_crashPath, pathUtf8.constData(), sizeof(g_crashPath) - 1);
  g_crashPath[sizeof(g_crashPath) - 1] = '\0';

  // Ensure parent dir exists (mkpath above is best-effort).
  // O_CREAT | O_WRONLY | O_TRUNC: fresh file each session.
  int fd = ::open(pathUtf8.constData(),
                  O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd < 0) {
    g_crashFd = -1;
    return;
  }
  g_crashFd = fd;
  // Write a header so an empty file (no crash) is distinguishable from a
  // file with crash info.
  safeWriteCStr(fd, "BLOP_DIAG_RUNNING ");
  const QByteArray ts =
      QDateTime::currentDateTimeUtc()
          .toString(Qt::ISODate)
          .toUtf8();
  safeWriteAll(fd, ts.constData(), (size_t)ts.size());
  safeWriteCStr(fd, "\n");
  ::fsync(fd);
#endif
}

} // namespace

void install() {
  bool expected = false;
  if (!g_installed.compare_exchange_strong(expected, true)) {
    return;
  }

  // Move any actual crash report from the previous run aside so this run's
  // file descriptor can truncate freely. takeCrashReportIfPresent() reads
  // the rotated previous_crash.txt later (after MainWindow is shown).
  rotatePreviousCrashFile();

  // Pre-open a fresh crash file for THIS run.
  openCrashFd();

  g_chainedQtHandler = qInstallMessageHandler(&qtMessageHandler);

  recordUiAction(QStringLiteral("blop_diag_installed"));

#if BLOP_DIAG_HAS_POSIX
  installSignalHandlers();
#endif
}

void recordUiAction(const QString &tag) {
  const QByteArray utf8 = tag.toUtf8();
  writeLineToRing("U", utf8.constData());
}

QString takeCrashReportIfPresent() {
  // We always read from the rotated previous_crash.txt, written by the
  // previous run and rotated by this run's install().
  const QString prev = previousCrashFilePath();
  QFile f(prev);
  if (!f.exists()) {
    return QString();
  }
  if (!f.open(QIODevice::ReadOnly)) {
    return QString();
  }
  const QByteArray data = f.readAll();
  f.close();

  if (!data.contains("==== BLOP CRASH ====")) {
    QFile::remove(prev);
    return QString();
  }

  QFile::remove(prev);
  return QString::fromUtf8(data);
}

QString crashReportPath() {
  return QString::fromUtf8(g_crashPath);
}

} // namespace BlopDiag
