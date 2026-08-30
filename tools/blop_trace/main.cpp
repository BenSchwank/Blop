// BlopTrace — opt-in QA console launcher (Windows).
// Starts Blop.exe with BLOP_SESSION_TRACE=1, tees session_trace.log live to
// this console and to Downloads\Blop_session_*.log. Ctrl+C finalizes the
// Downloads copy and exits. Separate EXE — normal users never run this.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::atomic<bool> g_stop{false};
HANDLE g_blopProc = nullptr;

std::string wideToUtf8(const std::wstring &s) {
  if (s.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, s.data(),
                                    static_cast<int>(s.size()), nullptr, 0,
                                    nullptr, nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()),
                      out.data(), n, nullptr, nullptr);
  return out;
}

std::wstring knownFolder(REFKNOWNFOLDERID id) {
  PWSTR path = nullptr;
  if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &path)) || !path)
    return {};
  std::wstring out(path);
  CoTaskMemFree(path);
  return out;
}

std::wstring exeDir() {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0 || n >= MAX_PATH)
    return L".";
  std::wstring p(buf, n);
  const size_t slash = p.find_last_of(L"\\/");
  if (slash == std::wstring::npos)
    return L".";
  return p.substr(0, slash);
}

std::wstring joinPath(const std::wstring &a, const std::wstring &b) {
  if (a.empty())
    return b;
  if (a.back() == L'\\' || a.back() == L'/')
    return a + b;
  return a + L'\\' + b;
}

bool fileExists(const std::wstring &path) {
  const DWORD attr = GetFileAttributesW(path.c_str());
  return attr != INVALID_FILE_ATTRIBUTES &&
         !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring timestampStamp() {
  SYSTEMTIME st{};
  GetLocalTime(&st);
  wchar_t buf[64];
  swprintf(buf, 64, L"%04u%02u%02u_%02u%02u%02u", st.wYear, st.wMonth,
           st.wDay, st.wHour, st.wMinute, st.wSecond);
  return buf;
}

std::vector<std::wstring> candidateSessionLogs() {
  std::vector<std::wstring> out;
  const std::wstring roaming = knownFolder(FOLDERID_RoamingAppData);
  const std::wstring local = knownFolder(FOLDERID_LocalAppData);
  const wchar_t *tails[] = {
      L"Blop\\BlopApp\\session_trace.log",
      L"Blop\\session_trace.log",
      L"benschwank\\Blop\\session_trace.log",
  };
  for (const auto &root : {roaming, local}) {
    if (root.empty())
      continue;
    for (const wchar_t *t : tails)
      out.push_back(joinPath(root, t));
  }
  return out;
}

std::wstring resolveSessionLogPath() {
  std::wstring best;
  FILETIME bestWrite{};
  bool haveBest = false;
  for (const auto &p : candidateSessionLogs()) {
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &fad))
      continue;
    if (!haveBest ||
        CompareFileTime(&fad.ftLastWriteTime, &bestWrite) > 0) {
      best = p;
      bestWrite = fad.ftLastWriteTime;
      haveBest = true;
    }
  }
  if (haveBest)
    return best;
  const std::wstring roaming = knownFolder(FOLDERID_RoamingAppData);
  if (!roaming.empty())
    return joinPath(roaming, L"Blop\\BlopApp\\session_trace.log");
  return joinPath(knownFolder(FOLDERID_LocalAppData),
                  L"Blop\\BlopApp\\session_trace.log");
}

void ensureParentDir(const std::wstring &filePath) {
  const size_t slash = filePath.find_last_of(L"\\/");
  if (slash == std::wstring::npos)
    return;
  SHCreateDirectoryExW(nullptr, filePath.substr(0, slash).c_str(), nullptr);
}

BOOL WINAPI consoleCtrlHandler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT ||
      type == CTRL_CLOSE_EVENT) {
    g_stop.store(true);
    return TRUE;
  }
  return FALSE;
}

bool launchBlop(const std::wstring &blopExe, HANDLE *outProc) {
  SetEnvironmentVariableW(L"BLOP_SESSION_TRACE", L"1");

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};

  std::wstring cmd = L"\"" + blopExe + L"\"";
  std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
  cmdBuf.push_back(L'\0');

  std::wstring workDir = blopExe;
  const size_t slash = workDir.find_last_of(L"\\/");
  if (slash != std::wstring::npos)
    workDir.resize(slash);

  const BOOL ok =
      CreateProcessW(blopExe.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                     0, nullptr, workDir.c_str(), &si, &pi);
  if (!ok) {
    std::wcerr << L"Fehler: Blop.exe Start fehlgeschlagen (GetLastError="
               << GetLastError() << L").\n";
    return false;
  }
  CloseHandle(pi.hThread);
  *outProc = pi.hProcess;
  g_blopProc = pi.hProcess;
  return true;
}

void teeLoop(const std::wstring &sessionPath, const std::wstring &downloadsPath) {
  ensureParentDir(downloadsPath);
  std::ofstream out(wideToUtf8(downloadsPath),
                    std::ios::binary | std::ios::trunc);
  if (!out) {
    std::wcerr << L"Konnte Downloads-Log nicht anlegen: " << downloadsPath
               << L"\n";
  } else {
    out << "=== BlopTrace session ===\n";
    out << "source=" << wideToUtf8(sessionPath) << "\n";
    out.flush();
  }

  std::ifstream in;
  std::streamoff offset = 0;
  bool openedOnce = false;

  while (!g_stop.load()) {
    if (!in.is_open()) {
      in.open(wideToUtf8(sessionPath), std::ios::binary);
      if (in) {
        if (!openedOnce) {
          std::wcout << L"\n--- Live-Trace (session_trace.log) ---\n";
          openedOnce = true;
        }
        offset = 0;
        in.seekg(0, std::ios::beg);
      }
    }

    if (in.is_open()) {
      in.clear();
      in.seekg(0, std::ios::end);
      const auto end = in.tellg();
      if (end >= 0 && end > offset) {
        in.seekg(offset);
        std::string chunk((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
        offset = end;
        if (!chunk.empty()) {
          std::cout << chunk << std::flush;
          if (out) {
            out << chunk;
            out.flush();
          }
        }
      } else if (end >= 0 && end < offset) {
        offset = 0;
        in.seekg(0);
      }
    }

    if (g_blopProc) {
      if (WaitForSingleObject(g_blopProc, 0) == WAIT_OBJECT_0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        if (in.is_open()) {
          in.clear();
          in.seekg(offset);
          std::string chunk((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
          if (!chunk.empty()) {
            std::cout << chunk << std::flush;
            if (out)
              out << chunk;
          }
        }
        g_stop.store(true);
        break;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
  }

  if (out) {
    out << "\n=== BlopTrace end (Ctrl+C or Blop exited) ===\n";
    out.flush();
  }
}

std::wstring firstArgBlopPath() {
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv)
    return {};
  std::wstring out;
  if (argc >= 2)
    out = argv[1];
  LocalFree(argv);
  return out;
}

} // namespace

int main() {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetConsoleCtrlHandler(consoleCtrlHandler, TRUE);

  std::wcout
      << L"BlopTrace — Session-Log fuer Entwickler/QA (opt-in)\n"
      << L"  • Startet Blop mit BLOP_SESSION_TRACE=1\n"
      << L"  • Live-Ausgabe hier in der Konsole\n"
      << L"  • Speichert fortlaufend nach Downloads\n"
      << L"  • Strg+C = Log finalisieren und beenden\n\n";

  std::wstring blopExe = firstArgBlopPath();
  if (blopExe.empty()) {
    const std::wstring beside = joinPath(exeDir(), L"Blop.exe");
    if (fileExists(beside)) {
      blopExe = beside;
    } else {
      // Standalone release asset: Blop lives in the normal install dir.
      const wchar_t *installCandidates[] = {
          L"C:\\Program Files\\Blop\\Blop.exe",
          L"C:\\Program Files (x86)\\Blop\\Blop.exe",
      };
      for (const wchar_t *c : installCandidates) {
        if (fileExists(c)) {
          blopExe = c;
          break;
        }
      }
      if (blopExe.empty())
        blopExe = beside; // keep error message path
    }
  }

  if (!fileExists(blopExe)) {
    std::wcerr << L"Blop.exe nicht gefunden: " << blopExe << L"\n"
               << L"Installer-Blop unter Program Files\\Blop, oder BlopTrace.exe "
                  L"neben Blop.exe legen, oder Pfad als Argument uebergeben.\n";
    std::wcout << L"\nTaste druecken zum Schliessen...";
    std::cin.get();
    return 1;
  }

  const std::wstring downloads = knownFolder(FOLDERID_Downloads);
  if (downloads.empty()) {
    std::wcerr << L"Downloads-Ordner nicht gefunden.\n";
    return 1;
  }
  const std::wstring outLog =
      joinPath(downloads, L"Blop_session_" + timestampStamp() + L".log");

  std::wcout << L"Blop:      " << blopExe << L"\n";
  std::wcout << L"Downloads: " << outLog << L"\n";

  if (!launchBlop(blopExe, &g_blopProc)) {
    std::wcout << L"\nTaste druecken zum Schliessen...";
    std::cin.get();
    return 1;
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(400));
  const std::wstring sessionPath = resolveSessionLogPath();
  std::wcout << L"AppData:   " << sessionPath << L"\n";
  std::wcout << L"\nBlop laeuft. Aktionen in der App erscheinen hier live.\n"
             << L"Strg+C speichert und beendet Trace.\n";

  teeLoop(sessionPath, outLog);

  if (g_blopProc) {
    if (WaitForSingleObject(g_blopProc, 0) != WAIT_OBJECT_0) {
      std::wcout << L"\nBeende Blop...\n";
      TerminateProcess(g_blopProc, 0);
      WaitForSingleObject(g_blopProc, 3000);
    }
    CloseHandle(g_blopProc);
    g_blopProc = nullptr;
  }

  {
    std::ifstream src(wideToUtf8(sessionPath), std::ios::binary);
    if (src) {
      const std::wstring finalCopy = joinPath(
          downloads, L"Blop_session_" + timestampStamp() + L"_final.log");
      std::ofstream dst(wideToUtf8(finalCopy), std::ios::binary);
      dst << src.rdbuf();
      dst.flush();
      std::wcout << L"\nLog gespeichert:\n  " << outLog << L"\n  " << finalCopy
                 << L"\n";
    } else {
      std::wcout << L"\nLog gespeichert:\n  " << outLog << L"\n";
    }
  }

  std::wcout << L"\nFertig. Fenster schliesst in 5 Sekunden (oder Taste)...\n";
  HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
  if (hIn && hIn != INVALID_HANDLE_VALUE) {
    FlushConsoleInputBuffer(hIn);
    WaitForSingleObject(hIn, 5000);
  } else {
    Sleep(2000);
  }
  return 0;
}
