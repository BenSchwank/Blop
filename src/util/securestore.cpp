#include "securestore.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
const QString kPrefix = QStringLiteral("dpapi1:");
}

namespace SecureStore {

QString protect(const QString &plain) {
  if (plain.isEmpty())
    return plain;
#ifdef Q_OS_WIN
  const QByteArray in = plain.toUtf8();
  DATA_BLOB inBlob;
  inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(in.constData()));
  inBlob.cbData = static_cast<DWORD>(in.size());
  DATA_BLOB outBlob{0, nullptr};
  if (CryptProtectData(&inBlob, L"Blop", nullptr, nullptr, nullptr,
                       CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
    const QByteArray enc(reinterpret_cast<const char *>(outBlob.pbData),
                         static_cast<qsizetype>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return kPrefix + QString::fromLatin1(enc.toBase64());
  }
#endif
  return plain;
}

QString unprotect(const QString &stored) {
  if (!stored.startsWith(kPrefix))
    return stored; // legacy plaintext (or non-Windows pass-through)
#ifdef Q_OS_WIN
  QByteArray enc = QByteArray::fromBase64(stored.mid(kPrefix.size()).toLatin1());
  DATA_BLOB inBlob;
  inBlob.pbData = reinterpret_cast<BYTE *>(enc.data());
  inBlob.cbData = static_cast<DWORD>(enc.size());
  DATA_BLOB outBlob{0, nullptr};
  if (CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
    const QString out = QString::fromUtf8(
        reinterpret_cast<const char *>(outBlob.pbData),
        static_cast<qsizetype>(outBlob.cbData));
    LocalFree(outBlob.pbData);
    return out;
  }
#endif
  return {}; // undecryptable (other user / other machine): treat as signed out
}

} // namespace SecureStore