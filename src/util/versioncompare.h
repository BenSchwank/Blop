#pragma once

#include <QList>
#include <QRegularExpression>
#include <QString>

/// Release version helpers shared by the in-app updater and the unit tests.
namespace BlopVersion {

/// "v3.29.47-3-gf390cd7" -> {3, 29, 47, 0}. Anything after the numeric
/// x[.y[.z[.w]]] prefix (git-describe suffix, "-beta", ...) is ignored.
inline QList<int> parse(const QString &version) {
  QString v = version.trimmed();
  if (v.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
    v.remove(0, 1);
  static const QRegularExpression re(
      QStringLiteral("^(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?(?:\\.(\\d+))?"));
  QList<int> out{0, 0, 0, 0};
  const QRegularExpressionMatch m = re.match(v);
  if (!m.hasMatch())
    return out;
  for (int i = 0; i < 4; ++i)
    out[i] = m.captured(i + 1).toInt();
  return out;
}

/// True when `remote` is a strictly higher release than `local`.
inline bool isNewer(const QString &remote, const QString &local) {
  const QList<int> r = parse(remote);
  const QList<int> l = parse(local);
  for (int i = 0; i < 4; ++i) {
    if (r[i] != l[i])
      return r[i] > l[i];
  }
  return false;
}

} // namespace BlopVersion