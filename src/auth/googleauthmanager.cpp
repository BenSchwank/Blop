#include "googleauthmanager.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QSettings>

#ifdef Q_OS_ANDROID
#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRandomGenerator>
#include <QSettings>
#include <QTimer>
#include <QtCore/qnativeinterface.h>
#include <QJniEnvironment>
#include <QJniObject>
#else
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#endif

namespace {
#ifdef Q_OS_ANDROID
// Android OAuth client (no client secret). Configured as "Android" client in
// Google Cloud Console with package com.benschwank.blop + Play App Signing
// SHA-1 (and Upload SHA-1).
//
// Google's authorized custom-scheme redirect for an Android/iOS client is
// ALWAYS the reverse client-id form:
//   com.googleusercontent.apps.<CLIENT_ID>:/oauth2redirect
// A package-name scheme (com.benschwank.blop:/…) is NOT authorized for this
// client type — Chrome then hangs on "Einen Moment bitte…" and drops the
// user on google.com instead of returning to the app (see user screenshots).
constexpr const char *kAndroidClientId =
    "571766217-5pcb10b1bgdv5g31vjgfvftdudufjc4s.apps.googleusercontent.com";
constexpr const char *kAndroidRedirectUri =
    "com.googleusercontent.apps.571766217-5pcb10b1bgdv5g31vjgfvftdudufjc4s:/"
    "oauth2redirect";
constexpr const char *kGoogleAuthEndpoint =
    "https://accounts.google.com/o/oauth2/v2/auth";
constexpr const char *kGoogleTokenEndpoint =
    "https://oauth2.googleapis.com/token";

constexpr const char *kPkceOrg = "Blop";
constexpr const char *kPkceApp = "BlopApp";
constexpr const char *kKeyPkceVerifier = "oauth/pkce_verifier";
constexpr const char *kKeyPkceState = "oauth/pkce_state";
constexpr const char *kKeyPkceStartedMs = "oauth/pkce_started_ms";

void persistPkce(const QString &verifier, const QString &state) {
  QSettings s(QString::fromLatin1(kPkceOrg), QString::fromLatin1(kPkceApp));
  s.setValue(QString::fromLatin1(kKeyPkceVerifier), verifier);
  s.setValue(QString::fromLatin1(kKeyPkceState), state);
  s.setValue(QString::fromLatin1(kKeyPkceStartedMs),
             QDateTime::currentMSecsSinceEpoch());
  s.sync();
}

void clearPersistedPkce() {
  QSettings s(QString::fromLatin1(kPkceOrg), QString::fromLatin1(kPkceApp));
  s.remove(QString::fromLatin1(kKeyPkceVerifier));
  s.remove(QString::fromLatin1(kKeyPkceState));
  s.remove(QString::fromLatin1(kKeyPkceStartedMs));
  s.sync();
}

bool restorePersistedPkce(QString *verifier, QString *state) {
  if (!verifier || !state)
    return false;
  QSettings s(QString::fromLatin1(kPkceOrg), QString::fromLatin1(kPkceApp));
  const QString v = s.value(QString::fromLatin1(kKeyPkceVerifier)).toString();
  const QString st = s.value(QString::fromLatin1(kKeyPkceState)).toString();
  const qint64 started =
      s.value(QString::fromLatin1(kKeyPkceStartedMs), 0).toLongLong();
  // Drop stale sessions older than 15 minutes.
  if (v.isEmpty() || st.isEmpty() || started <= 0)
    return false;
  if (QDateTime::currentMSecsSinceEpoch() - started > 15 * 60 * 1000) {
    clearPersistedPkce();
    return false;
  }
  *verifier = v;
  *state = st;
  return true;
}

// Bridge from BlopOAuthBridge (Java) into the Qt singleton.
// Registered via JNI so symbols resolve before the custom tab redirects.
extern "C" JNIEXPORT void JNICALL
Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthCallback(
    JNIEnv *env, jclass /*clazz*/, jstring uri) {
  if (!uri) {
    qWarning() << "BlopOAuthBridge.notifyAuthCallback: null uri";
    return;
  }
  const char *raw = env->GetStringUTFChars(uri, nullptr);
  const QString s = QString::fromUtf8(raw ? raw : "");
  env->ReleaseStringUTFChars(uri, raw);
  GoogleAuthManager::instance().handleDeepLinkCallback(s);
}

extern "C" JNIEXPORT void JNICALL
Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthResume(
    JNIEnv * /*env*/, jclass /*clazz*/) {
  GoogleAuthManager::instance().handleExternalAuthResume();
}

extern "C" JNIEXPORT void JNICALL
Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthAbandoned(
    JNIEnv *env, jclass /*clazz*/, jstring reason) {
  QString s = QStringLiteral("browser_open_failed");
  if (reason) {
    const char *raw = env->GetStringUTFChars(reason, nullptr);
    s = QString::fromUtf8(raw ? raw : "");
    env->ReleaseStringUTFChars(reason, raw);
    if (s.isEmpty())
      s = QStringLiteral("browser_open_failed");
  }
  GoogleAuthManager::instance().handleExternalAuthAbandoned(s);
}
#else
// Render-hosted GIS bridge on the authorized blop-study.com origin (sign-in).
constexpr const char *kDesktopBridgeUrl =
    "https://www.blop-study.com/api/auth/google/desktop/bridge";
constexpr const char *kDesktopClaimUrl =
    "https://www.blop-study.com/api/auth/google/desktop/claim";
constexpr const char *kDesktopExchangeUrl =
    "https://www.blop-study.com/api/auth/google/desktop/exchange";
// Legacy OAuth client used for Calendar loopback. Google's token endpoint
// for this client requires client_secret — exchange goes via blop-study.com
// (or BLOP_GOOGLE_CLIENT_SECRET for local/dev).
constexpr const char *kDesktopOAuthClientId =
    "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com";
constexpr const char *kGoogleAuthEndpoint =
    "https://accounts.google.com/o/oauth2/v2/auth";
constexpr const char *kGoogleTokenEndpoint =
    "https://oauth2.googleapis.com/token";

QByteArray loadDesktopClientSecret() {
  const QByteArray fromEnv = qgetenv("BLOP_GOOGLE_CLIENT_SECRET");
  if (!fromEnv.trimmed().isEmpty())
    return fromEnv.trimmed();

  // Same folder as scripts/setup-google-desktop-secret.ps1 writes to:
  // %APPDATA%\Blop\BlopApp\google_desktop_client_secret.txt
  QString path;
#ifdef Q_OS_WIN
  const QByteArray appdata = qgetenv("APPDATA");
  if (!appdata.isEmpty()) {
    path = QDir(QString::fromLocal8Bit(appdata))
               .filePath(QStringLiteral("Blop/BlopApp/google_desktop_client_secret.txt"));
  }
#endif
  if (path.isEmpty()) {
    path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
               .filePath(QStringLiteral("google_desktop_client_secret.txt"));
  }
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};
  return f.readAll().trimmed();
}
#endif // Q_OS_ANDROID
} // namespace

GoogleAuthManager &GoogleAuthManager::instance() {
  static GoogleAuthManager instance;
  return instance;
}

GoogleAuthManager::GoogleAuthManager(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
  m_networkManager = new QNetworkAccessManager(this);
  m_clientId = QString::fromLatin1(kAndroidClientId);
  m_redirectUri = QString::fromLatin1(kAndroidRedirectUri);

  // Register the native callback so Java BlopOAuthBridge can call into us once
  // the deep link arrives (and when the activity resumes without one).
  static bool s_jniRegistered = false;
  if (!s_jniRegistered) {
    QJniEnvironment env;
    if (env.registerNativeMethods(
            "com/benschwank/blop/BlopOAuthBridge",
            {{"nativeNotifyAuthCallback", "(Ljava/lang/String;)V",
              reinterpret_cast<void *>(
                  &Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthCallback)},
             {"nativeNotifyAuthResume", "()V",
              reinterpret_cast<void *>(
                  &Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthResume)},
             {"nativeNotifyAuthAbandoned", "(Ljava/lang/String;)V",
              reinterpret_cast<void *>(
                  &Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthAbandoned)}})) {
      s_jniRegistered = true;
      qInfo() << "GoogleAuthManager: registered BlopOAuthBridge JNI methods";
    } else {
      qWarning() << "GoogleAuthManager: failed to register BlopOAuthBridge JNI"
                    " methods (deep-link callback will not work)";
    }
  }

  // Drain any auth URI that arrived before the singleton was fully constructed.
  // Defer to the next event-loop tick so MainWindow can finish connecting to
  // authenticationFailed / idTokenReceived first (otherwise a cold-start
  // callback during instance() construction would emit into the void).
  QJniObject pending = QJniObject::callStaticObjectMethod(
      "com/benschwank/blop/BlopOAuthBridge", "consumePendingUri",
      "()Ljava/lang/String;");
  if (pending.isValid()) {
    const QString s = pending.toString();
    if (!s.isEmpty()) {
      QTimer::singleShot(0, this, [this, s]() { handleDeepLinkCallback(s); });
    }
  }
#else
  m_networkManager = new QNetworkAccessManager(this);
  m_bridgeTimeout = new QTimer(this);
  m_bridgeTimeout->setSingleShot(true);
  connect(m_bridgeTimeout, &QTimer::timeout, this, [this]() {
    if (!m_loginInProgress)
      return;
    if (m_desktopPkceActive) {
      qWarning() << "GoogleAuthManager: desktop Calendar PKCE timed out";
      cancelPendingLogin();
      emit authenticationFailed(QStringLiteral("oauth_pkce_timeout"));
      return;
    }
    qWarning() << "GoogleAuthManager: desktop bridge timed out";
    finishDesktopBridge(QString(), QStringLiteral("oauth_bridge_timeout"));
  });
  m_bridgePoll = new QTimer(this);
  m_bridgePoll->setInterval(1000);
  connect(m_bridgePoll, &QTimer::timeout, this,
          &GoogleAuthManager::pollDesktopClaim);
#endif
  loadPersistedAccessToken();
}

void GoogleAuthManager::login() {
#ifdef Q_OS_ANDROID
  startPkceLogin();
#else
  // Calendar needs a real OAuth access_token; GIS bridge on production only
  // returns an id_token until the calendar=1 deploy is live.
  if (m_wantCalendar)
    startDesktopCalendarPkceLogin();
  else
    startDesktopBridgeLogin();
#endif
}

void GoogleAuthManager::loginForCalendar() {
  m_wantCalendar = true;
  login();
}

bool GoogleAuthManager::hasCalendarAccess() const {
  return !m_accessToken.isEmpty();
}

QString GoogleAuthManager::accessToken() const { return m_accessToken; }

void GoogleAuthManager::clearCalendarAccess() {
  persistAccessToken(QString());
}

void GoogleAuthManager::persistAccessToken(const QString &token) {
  m_accessToken = token;
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  if (token.isEmpty())
    st.remove(QStringLiteral("google/access_token"));
  else
    st.setValue(QStringLiteral("google/access_token"), token);
  emit calendarTokenUpdated();
}

void GoogleAuthManager::loadPersistedAccessToken() {
  QSettings st(QStringLiteral("Blop"), QStringLiteral("BlopApp"));
  m_accessToken = st.value(QStringLiteral("google/access_token")).toString();
}

void GoogleAuthManager::parseUserInfoFromIdToken(const QString &idToken) {
  // Google id_token is a JWT (header.payload.signature). Decode the payload
  // best-effort to populate display info; signature verification happens on the
  // backend in /api/auth/google/verify.
  const QStringList parts = idToken.split('.');
  if (parts.size() < 2)
    return;
  QByteArray payload = QByteArray::fromBase64(
      parts.at(1).toUtf8(), QByteArray::Base64UrlEncoding);
  if (payload.isEmpty())
    return;
  const QJsonDocument doc = QJsonDocument::fromJson(payload);
  if (!doc.isObject())
    return;
  const QJsonObject obj = doc.object();
  m_email = obj.value("email").toString();
  m_name = obj.value("name").toString(m_email);
  m_pictureUrl = obj.value("picture").toString();
  if (!m_email.isEmpty())
    emit userInfoUpdated();
}

#ifdef Q_OS_ANDROID
void GoogleAuthManager::cancelPendingLogin() {
  if (!m_loginInProgress && m_pkceVerifier.isEmpty() && m_pkceState.isEmpty())
    return;
  qInfo() << "GoogleAuthManager: cancelling pending PKCE login on caller request";
  ++m_authResumeGeneration; // invalidate any pending resume-grace timer
  m_loginInProgress = false;
  m_loginInProgressSinceMs = 0;
  clearPersistedPkce();
  m_pkceVerifier.clear();
  m_pkceState.clear();
  QJniObject::callStaticMethod<void>(
      "com/benschwank/blop/BlopOAuthBridge", "clearExternalAuthPending", "()V");
}

QString GoogleAuthManager::generateRandomString(int length) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  QString out;
  out.reserve(length);
  for (int i = 0; i < length; ++i) {
    const quint32 idx =
        QRandomGenerator::system()->bounded(quint32(sizeof(alphabet) - 1));
    out.append(QChar::fromLatin1(alphabet[idx]));
  }
  return out;
}

QString GoogleAuthManager::base64UrlEncode(const QByteArray &data) {
  QByteArray b64 = data.toBase64(QByteArray::Base64UrlEncoding |
                                 QByteArray::OmitTrailingEquals);
  return QString::fromLatin1(b64);
}

void GoogleAuthManager::startPkceLogin() {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_loginInProgress) {
    // Allow long Google sessions (account picker / 2FA). Only treat as stale
    // after 10 minutes so a mid-flow retry does not wipe PKCE state.
    const qint64 ageMs = nowMs - m_loginInProgressSinceMs;
    if (ageMs > 10 * 60 * 1000) {
      qInfo() << "GoogleAuthManager: stale PKCE login lock (" << ageMs
              << "ms old) — clearing and restarting";
      cancelPendingLogin();
    } else {
      qInfo() << "GoogleAuthManager: PKCE login already in progress (started "
              << ageMs << "ms ago)";
      return;
    }
  }
  m_loginInProgress = true;
  m_loginInProgressSinceMs = nowMs;
  ++m_authResumeGeneration;

  // PKCE: generate a high-entropy code_verifier and S256-hashed challenge.
  m_pkceVerifier = generateRandomString(64);
  const QByteArray challenge = QCryptographicHash::hash(
      m_pkceVerifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge = base64UrlEncode(challenge);
  m_pkceState = generateRandomString(32);
  // Survive process death while the Custom Tab is open (common on low-RAM
  // phones). Without this, onNewIntent returns with empty in-memory state
  // → oauth_state_mismatch.
  persistPkce(m_pkceVerifier, m_pkceState);

  QUrl authUrl(QString::fromLatin1(kGoogleAuthEndpoint));
  QUrlQuery q;
  q.addQueryItem("response_type", "code");
  q.addQueryItem("client_id", m_clientId);
  q.addQueryItem("redirect_uri", m_redirectUri);
  QString scopes = QStringLiteral("openid email profile");
  if (m_wantCalendar) {
    scopes += QStringLiteral(
        " https://www.googleapis.com/auth/calendar.readonly"
        " https://www.googleapis.com/auth/calendar.events");
    q.addQueryItem("access_type", "offline");
    q.addQueryItem("prompt", "consent");
  } else {
    q.addQueryItem("prompt", "select_account");
  }
  q.addQueryItem("scope", scopes);
  q.addQueryItem("code_challenge", codeChallenge);
  q.addQueryItem("code_challenge_method", "S256");
  q.addQueryItem("state", m_pkceState);
  authUrl.setQuery(q);

  qInfo() << "GoogleAuthManager: launching PKCE auth via Custom Tab"
          << "redirect=" << m_redirectUri;
  emit loginPhaseChanged(QStringLiteral("browser"));
  emit requireBrowser(authUrl);
}

void GoogleAuthManager::handleExternalAuthResume() {
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(this, [this]() { handleExternalAuthResume(); },
                              Qt::QueuedConnection);
    return;
  }
  if (!m_loginInProgress) {
    qInfo() << "GoogleAuthManager: auth resume ignored (no login in progress)";
    return;
  }
  // Deep link often arrives slightly after onResume (redirect trampoline →
  // BlopActivity). Wait long enough so a successful redirect is not falsely
  // treated as cancel; user account-picker / 2FA returns are also covered.
  const int gen = ++m_authResumeGeneration;
  qInfo() << "GoogleAuthManager: activity resumed during PKCE — grace 12000ms"
          << "gen=" << gen;
  QTimer::singleShot(12000, this, [this, gen]() {
    if (gen != m_authResumeGeneration)
      return;
    if (!m_loginInProgress)
      return;
    qWarning() << "GoogleAuthManager: no OAuth redirect after resume — abandoning";
    cancelPendingLogin();
    emit authenticationFailed(QStringLiteral("oauth_redirect_missing"));
  });
}

void GoogleAuthManager::handleExternalAuthAbandoned(const QString &reason) {
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(
        this, [this, reason]() { handleExternalAuthAbandoned(reason); },
        Qt::QueuedConnection);
    return;
  }
  qWarning() << "GoogleAuthManager: external auth abandoned:" << reason;
  cancelPendingLogin();
  emit authenticationFailed(reason.isEmpty()
                                ? QStringLiteral("browser_open_failed")
                                : reason);
}

void GoogleAuthManager::handleDeepLinkCallback(const QString &uri) {
  // This method may be called from the Android main thread (via JNI). All Qt
  // object work (QNetworkAccessManager, signals) must run on the Qt thread.
  if (QThread::currentThread() != thread()) {
    QMetaObject::invokeMethod(this, [this, uri]() {
      handleDeepLinkCallback(uri);
    }, Qt::QueuedConnection);
    return;
  }

  qInfo() << "GoogleAuthManager: deep-link callback received uri=" << uri;
  ++m_authResumeGeneration; // cancel any pending "redirect missing" timer
  QJniObject::callStaticMethod<void>(
      "com/benschwank/blop/BlopOAuthBridge", "clearExternalAuthPending", "()V");
  emit redirectReceived();

  qInfo() << "GoogleAuthManager: deep-link accepted, exchanging code";
  if (uri.isEmpty()) {
    m_loginInProgress = false;
    clearPersistedPkce();
    emit authenticationFailed(QStringLiteral("empty_callback_uri"));
    return;
  }

  // Accept both path-form and host-form redirects; pull query robustly.
  QUrl parsed(uri, QUrl::TolerantMode);
  QUrlQuery q(parsed);
  if (q.isEmpty()) {
    const int qi = uri.indexOf(QLatin1Char('?'));
    if (qi >= 0)
      q = QUrlQuery(uri.mid(qi + 1));
  }

  const QString error = q.queryItemValue(QStringLiteral("error"));
  if (!error.isEmpty()) {
    qWarning() << "OAuth provider returned error:" << error;
    m_loginInProgress = false;
    clearPersistedPkce();
    emit authenticationFailed(QStringLiteral("oauth_error:") + error);
    return;
  }

  // Restore PKCE if the process was killed while the Custom Tab was open.
  if (m_pkceState.isEmpty() || m_pkceVerifier.isEmpty()) {
    QString v, st;
    if (restorePersistedPkce(&v, &st)) {
      qInfo() << "GoogleAuthManager: restored PKCE state after process restart";
      m_pkceVerifier = v;
      m_pkceState = st;
    }
  }

  const QString state = q.queryItemValue(QStringLiteral("state"));
  if (state.isEmpty() || state != m_pkceState) {
    qWarning() << "OAuth state mismatch (mem empty=" << m_pkceState.isEmpty()
               << "callbackStateEmpty=" << state.isEmpty() << ")";
    m_loginInProgress = false;
    clearPersistedPkce();
    emit authenticationFailed(QStringLiteral("oauth_state_mismatch"));
    return;
  }

  const QString code = q.queryItemValue(QStringLiteral("code"));
  if (code.isEmpty()) {
    qWarning() << "OAuth callback missing code";
    m_loginInProgress = false;
    clearPersistedPkce();
    emit authenticationFailed(QStringLiteral("oauth_missing_code"));
    return;
  }

  emit loginPhaseChanged(QStringLiteral("callback"));
  exchangeAuthorizationCode(code);
}

void GoogleAuthManager::exchangeAuthorizationCode(const QString &code) {
  emit loginPhaseChanged(QStringLiteral("verify"));
  QNetworkRequest req((QUrl(QString::fromLatin1(kGoogleTokenEndpoint))));
  req.setHeader(QNetworkRequest::ContentTypeHeader,
                "application/x-www-form-urlencoded");

  QUrlQuery body;
  body.addQueryItem("grant_type", "authorization_code");
  body.addQueryItem("code", code);
  body.addQueryItem("redirect_uri", m_redirectUri);
  body.addQueryItem("client_id", m_clientId);
  body.addQueryItem("code_verifier", m_pkceVerifier);

  const QByteArray payload = body.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = m_networkManager->post(req, payload);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const QByteArray raw = reply->readAll();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto netErr = reply->error();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError || status >= 400) {
      qWarning() << "Google token exchange failed status=" << status
                 << "body=" << raw;
      m_loginInProgress = false;
      clearPersistedPkce();
      emit authenticationFailed(QStringLiteral("token_exchange_failed"));
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
      qWarning() << "Google token response not JSON object:" << raw;
      m_loginInProgress = false;
      clearPersistedPkce();
      emit authenticationFailed(QStringLiteral("token_exchange_invalid_json"));
      return;
    }
    const QJsonObject obj = doc.object();
    const QString idToken = obj.value("id_token").toString();
    const QString accessToken = obj.value("access_token").toString();
    if (idToken.isEmpty()) {
      qWarning() << "Google token response missing id_token";
      m_loginInProgress = false;
      clearPersistedPkce();
      emit authenticationFailed(QStringLiteral("token_exchange_no_id_token"));
      return;
    }

    clearPersistedPkce();
    m_loginInProgress = false;
    if (!accessToken.isEmpty())
      persistAccessToken(accessToken);
    m_wantCalendar = false;
    parseUserInfoFromIdToken(idToken);
    m_authenticated = true;
    emit loginPhaseChanged(QStringLiteral("done"));
    emit idTokenReceived(idToken);
    emit authenticated();
  });
}
#else // Desktop
QString GoogleAuthManager::generateRandomString(int length) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  QString out;
  out.reserve(length);
  for (int i = 0; i < length; ++i) {
    const quint32 idx =
        QRandomGenerator::system()->bounded(quint32(sizeof(alphabet) - 1));
    out.append(QChar::fromLatin1(alphabet[idx]));
  }
  return out;
}

QString GoogleAuthManager::base64UrlEncode(const QByteArray &data) {
  QByteArray b64 = data.toBase64(QByteArray::Base64UrlEncoding |
                                 QByteArray::OmitTrailingEquals);
  return QString::fromLatin1(b64);
}

void GoogleAuthManager::stopDesktopLoopbackServer() {
  if (!m_loopbackServer)
    return;
  m_loopbackServer->close();
  m_loopbackServer->deleteLater();
  m_loopbackServer = nullptr;
}

void GoogleAuthManager::cancelPendingLogin() {
  if (!m_loginInProgress && !m_desktopPkceActive)
    return;
  qInfo() << "GoogleAuthManager: cancelling desktop login";
  stopDesktopBridgeTimers();
  stopDesktopLoopbackServer();
  m_loginInProgress = false;
  m_loginInProgressSinceMs = 0;
  m_bridgeState.clear();
  m_claimInFlight = false;
  m_desktopPkceActive = false;
  m_pkceVerifier.clear();
  m_pkceState.clear();
  m_redirectUri.clear();
  m_wantCalendar = false;
}

void GoogleAuthManager::stopDesktopBridgeTimers() {
  if (m_bridgeTimeout)
    m_bridgeTimeout->stop();
  if (m_bridgePoll)
    m_bridgePoll->stop();
}

void GoogleAuthManager::startDesktopCalendarPkceLogin() {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_loginInProgress) {
    const qint64 ageMs = nowMs - m_loginInProgressSinceMs;
    if (ageMs > 10 * 60 * 1000) {
      qInfo() << "GoogleAuthManager: stale desktop login lock — restarting";
      cancelPendingLogin();
    } else {
      qInfo() << "GoogleAuthManager: desktop login already in progress";
      return;
    }
  }

  stopDesktopBridgeTimers();
  stopDesktopLoopbackServer();
  m_claimInFlight = false;
  m_bridgeState.clear();

  m_loopbackServer = new QTcpServer(this);
  // Prefer a stable loopback port (easier to match Chrome's address bar / firewall).
  // Fall back to an ephemeral port if busy.
  constexpr quint16 kPreferredPort = 27183;
  if (!m_loopbackServer->listen(QHostAddress(QStringLiteral("127.0.0.1")),
                                kPreferredPort) &&
      !m_loopbackServer->listen(QHostAddress(QStringLiteral("127.0.0.1")), 0)) {
    qWarning() << "GoogleAuthManager: loopback listen failed"
               << m_loopbackServer->errorString();
    stopDesktopLoopbackServer();
    m_wantCalendar = false;
    emit authenticationFailed(QStringLiteral("oauth_loopback_listen_failed"));
    return;
  }
  const quint16 port = m_loopbackServer->serverPort();
  m_redirectUri =
      QStringLiteral("http://127.0.0.1:%1/").arg(port);
  m_clientId = QString::fromLatin1(kDesktopOAuthClientId);
  m_pkceVerifier = generateRandomString(64);
  m_pkceState = generateRandomString(32);
  const QByteArray challenge = QCryptographicHash::hash(
      m_pkceVerifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge = base64UrlEncode(challenge);

  m_desktopPkceActive = true;
  m_loginInProgress = true;
  m_loginInProgressSinceMs = nowMs;
  if (m_bridgeTimeout)
    m_bridgeTimeout->start(10 * 60 * 1000); // allow slow Google consent / 2FA

  // Avoid duplicate slots if login is retried without a full process restart.
  disconnect(m_loopbackServer, &QTcpServer::newConnection, this,
             &GoogleAuthManager::onDesktopLoopbackConnection);
  connect(m_loopbackServer, &QTcpServer::newConnection, this,
          &GoogleAuthManager::onDesktopLoopbackConnection);

  QUrl authUrl(QString::fromLatin1(kGoogleAuthEndpoint));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  q.addQueryItem(QStringLiteral("client_id"), m_clientId);
  q.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
  q.addQueryItem(
      QStringLiteral("scope"),
      QStringLiteral(
          "openid email profile "
          "https://www.googleapis.com/auth/calendar.readonly "
          "https://www.googleapis.com/auth/calendar.events"));
  q.addQueryItem(QStringLiteral("access_type"), QStringLiteral("offline"));
  q.addQueryItem(QStringLiteral("prompt"), QStringLiteral("consent"));
  q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
  q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
  q.addQueryItem(QStringLiteral("state"), m_pkceState);
  authUrl.setQuery(q);

  qInfo() << "GoogleAuthManager: opening desktop Calendar PKCE"
          << "listening=" << m_loopbackServer->isListening()
          << "redirect=" << m_redirectUri
          << "(Chrome must open this exact host:port — keep Blop running,"
             " prefer Run without debugger)";
  emit requireBrowser(authUrl);
}

void GoogleAuthManager::onDesktopLoopbackConnection() {
  if (!m_loopbackServer || !m_desktopPkceActive)
    return;
  QTcpSocket *sock = m_loopbackServer->nextPendingConnection();
  if (!sock)
    return;

  qInfo() << "GoogleAuthManager: loopback connection from"
          << sock->peerAddress().toString();

  // Don't rely only on readyRead — data may already be buffered before the
  // slot is connected (common with fast localhost redirects).
  auto handleRequest = [this, sock]() {
    if (!sock)
      return;
    if (!m_desktopPkceActive) {
      sock->disconnectFromHost();
      sock->deleteLater();
      return;
    }
    if (sock->bytesAvailable() <= 0 && !sock->waitForReadyRead(8000)) {
      qWarning() << "GoogleAuthManager: loopback got no HTTP data";
      sock->disconnectFromHost();
      sock->deleteLater();
      return;
    }
    const QByteArray raw = sock->readAll();
    const QString req = QString::fromUtf8(raw);
    const int lineEnd = req.indexOf(QStringLiteral("\r\n"));
    const QString firstLine =
        lineEnd > 0 ? req.left(lineEnd) : req.section(QLatin1Char('\n'), 0, 0);

    QString pathAndQuery;
    const QStringList parts = firstLine.split(QLatin1Char(' '));
    if (parts.size() >= 2)
      pathAndQuery = parts.at(1);

    QUrl cb(QStringLiteral("http://127.0.0.1") + pathAndQuery);
    QUrlQuery q(cb);
    const QString err = q.queryItemValue(QStringLiteral("error"));
    const QString state = q.queryItemValue(QStringLiteral("state"));
    const QString code = q.queryItemValue(QStringLiteral("code"));

    const QByteArray body =
        "<!DOCTYPE html><html><body style='font-family:sans-serif;padding:2rem'>"
        "<h2>Blop</h2><p>Anmeldung fertig — dieses Fenster kannst du "
        "schlie&szlig;en.</p></body></html>";
    const QByteArray resp =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n"
        "Content-Length: " +
        QByteArray::number(body.size()) + "\r\n\r\n" + body;
    sock->write(resp);
    sock->waitForBytesWritten(2000);
    sock->disconnectFromHost();
    sock->deleteLater();
    stopDesktopLoopbackServer();

    if (!err.isEmpty()) {
      qWarning() << "GoogleAuthManager: calendar PKCE provider error" << err;
      m_desktopPkceActive = false;
      m_loginInProgress = false;
      m_wantCalendar = false;
      emit authenticationFailed(QStringLiteral("oauth_error:") + err);
      return;
    }
    if (state != m_pkceState) {
      qWarning() << "GoogleAuthManager: calendar PKCE state mismatch";
      m_desktopPkceActive = false;
      m_loginInProgress = false;
      m_wantCalendar = false;
      emit authenticationFailed(QStringLiteral("oauth_state_mismatch"));
      return;
    }
    if (code.isEmpty()) {
      qWarning() << "GoogleAuthManager: calendar PKCE missing code";
      m_desktopPkceActive = false;
      m_loginInProgress = false;
      m_wantCalendar = false;
      emit authenticationFailed(QStringLiteral("oauth_missing_code"));
      return;
    }
    exchangeDesktopAuthorizationCode(code);
  };

  if (sock->bytesAvailable() > 0)
    QTimer::singleShot(0, this, handleRequest);
  else
    connect(sock, &QTcpSocket::readyRead, this, handleRequest,
            Qt::SingleShotConnection);
}

void GoogleAuthManager::exchangeDesktopAuthorizationCode(const QString &code) {
  if (!m_networkManager) {
    emit authenticationFailed(QStringLiteral("oauth_no_network"));
    return;
  }

  const QString verifier = m_pkceVerifier;
  const QString redirect = m_redirectUri;
  const QString clientId = m_clientId;

  auto finishOk = [this](const QString &accessToken, const QString &idToken) {
    stopDesktopBridgeTimers();
    m_desktopPkceActive = false;
    m_loginInProgress = false;
    m_loginInProgressSinceMs = 0;
    m_wantCalendar = false;
    m_pkceVerifier.clear();
    m_pkceState.clear();

    if (accessToken.isEmpty()) {
      emit authenticationFailed(QStringLiteral("token_exchange_no_access_token"));
      return;
    }
    persistAccessToken(accessToken);
    qInfo() << "GoogleAuthManager: desktop Calendar OAuth got access_token";
    if (!idToken.isEmpty()) {
      parseUserInfoFromIdToken(idToken);
      m_authenticated = true;
      emit idTokenReceived(idToken);
      emit authenticated();
    } else {
      emit calendarTokenUpdated();
    }
  };

  auto finishFail = [this](const QString &reason, const QByteArray &raw) {
    stopDesktopBridgeTimers();
    m_desktopPkceActive = false;
    m_loginInProgress = false;
    m_loginInProgressSinceMs = 0;
    m_wantCalendar = false;
    m_pkceVerifier.clear();
    m_pkceState.clear();
    qWarning() << "GoogleAuthManager: desktop token exchange failed:" << reason
               << "body=" << raw;
    emit authenticationFailed(reason);
  };

  // 1) Prefer server-side exchange (client_secret stays on Render).
  {
    QJsonObject payload;
    payload.insert(QStringLiteral("code"), code);
    payload.insert(QStringLiteral("code_verifier"), verifier);
    payload.insert(QStringLiteral("redirect_uri"), redirect);
    payload.insert(QStringLiteral("client_id"), clientId);
    QNetworkRequest req((QUrl(QString::fromLatin1(kDesktopExchangeUrl))));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("BlopDesktop/GoogleAuthExchange"));
    QNetworkReply *reply = m_networkManager->post(
        req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, code, verifier, redirect, clientId, finishOk,
             finishFail]() {
              const QByteArray raw = reply->readAll();
              const int status =
                  reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                      .toInt();
              const auto netErr = reply->error();
              reply->deleteLater();

              if (netErr == QNetworkReply::NoError && status >= 200 &&
                  status < 300) {
                const QJsonObject obj =
                    QJsonDocument::fromJson(raw).object();
                finishOk(obj.value(QStringLiteral("access_token")).toString(),
                         obj.value(QStringLiteral("id_token")).toString());
                return;
              }

              // 2) Local/dev fallback: direct Google token call with secret
              //    from env (BLOP_GOOGLE_CLIENT_SECRET) when backend is not
              //    deployed yet or missing the secret.
              const QByteArray secret = loadDesktopClientSecret();
              if (secret.isEmpty()) {
                finishFail(
                    status == 404
                        ? QStringLiteral("token_exchange_backend_missing")
                        : QStringLiteral("token_exchange_need_client_secret"),
                    raw);
                return;
              }

              qInfo() << "GoogleAuthManager: backend exchange unavailable — "
                         "trying direct token call with local client_secret";
              QNetworkRequest treq(
                  (QUrl(QString::fromLatin1(kGoogleTokenEndpoint))));
              treq.setHeader(QNetworkRequest::ContentTypeHeader,
                             QStringLiteral("application/x-www-form-urlencoded"));
              QUrlQuery body;
              body.addQueryItem(QStringLiteral("grant_type"),
                                QStringLiteral("authorization_code"));
              body.addQueryItem(QStringLiteral("code"), code);
              body.addQueryItem(QStringLiteral("redirect_uri"), redirect);
              body.addQueryItem(QStringLiteral("client_id"), clientId);
              body.addQueryItem(QStringLiteral("code_verifier"), verifier);
              body.addQueryItem(QStringLiteral("client_secret"),
                                QString::fromUtf8(secret));
              QNetworkReply *treply = m_networkManager->post(
                  treq, body.toString(QUrl::FullyEncoded).toUtf8());
              connect(treply, &QNetworkReply::finished, this,
                      [treply, finishOk, finishFail]() {
                        const QByteArray traw = treply->readAll();
                        const int tstatus =
                            treply
                                ->attribute(
                                    QNetworkRequest::HttpStatusCodeAttribute)
                                .toInt();
                        const auto terr = treply->error();
                        treply->deleteLater();
                        if (terr != QNetworkReply::NoError || tstatus >= 400) {
                          finishFail(QStringLiteral("token_exchange_failed"),
                                     traw);
                          return;
                        }
                        const QJsonObject obj =
                            QJsonDocument::fromJson(traw).object();
                        finishOk(
                            obj.value(QStringLiteral("access_token")).toString(),
                            obj.value(QStringLiteral("id_token")).toString());
                      });
            });
  }
}

void GoogleAuthManager::startDesktopBridgeLogin() {
  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_loginInProgress) {
    const qint64 ageMs = nowMs - m_loginInProgressSinceMs;
    if (ageMs > 10 * 60 * 1000) {
      qInfo() << "GoogleAuthManager: stale desktop bridge lock — restarting";
      cancelPendingLogin();
    } else {
      qInfo() << "GoogleAuthManager: desktop bridge already in progress";
      return;
    }
  }

  stopDesktopLoopbackServer();
  m_desktopPkceActive = false;
  m_bridgeState = generateRandomString(32);
  stopDesktopBridgeTimers();
  m_claimInFlight = false;
  m_loginInProgress = true;
  m_loginInProgressSinceMs = nowMs;
  m_bridgeTimeout->start(5 * 60 * 1000);
  m_bridgePoll->start();

  QUrl url(QString::fromLatin1(kDesktopBridgeUrl));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("state"), m_bridgeState);
  url.setQuery(q);

  qInfo() << "GoogleAuthManager: opening desktop GIS bridge (poll claim)"
          << "url=" << url.toString();
  emit requireBrowser(url);
}

void GoogleAuthManager::pollDesktopClaim() {
  if (!m_loginInProgress || m_bridgeState.isEmpty() || m_desktopPkceActive)
    return;
  claimDesktopState(m_bridgeState, false);
}

void GoogleAuthManager::handleDesktopOAuthDeepLink(const QUrl &url) {
  // Expected: blop://oauth/done?state=...  (or error=... from the provider).
  if (m_desktopPkceActive)
    return; // Calendar PKCE uses loopback, not blop://
  QUrlQuery q(url);
  const QString error = q.queryItemValue(QStringLiteral("error"));
  if (!error.isEmpty()) {
    const QString desc = q.queryItemValue(QStringLiteral("error_description"));
    qWarning() << "GoogleAuthManager: desktop OAuth provider error:" << error
               << desc;
    emit authenticationFailed(QStringLiteral("oauth_error:") + error +
                              (desc.isEmpty()
                                   ? QString()
                                   : QStringLiteral(" - ") + desc));
    return;
  }

  QString state = q.queryItemValue(QStringLiteral("state"));
  if (state.isEmpty()) {
    QUrlQuery pathQuery(url.query());
    state = pathQuery.queryItemValue(QStringLiteral("state"));
  }
  if (state.isEmpty()) {
    qWarning() << "GoogleAuthManager: deep link missing state" << url;
    return;
  }

  const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
  if (state == m_lastHandledDeepLinkState &&
      nowMs - m_lastHandledDeepLinkMs < 4000) {
    return;
  }
  m_lastHandledDeepLinkState = state;
  m_lastHandledDeepLinkMs = nowMs;

  qInfo() << "GoogleAuthManager: desktop OAuth deep link state=" << state;

  if (!m_loginInProgress) {
    qInfo() << "GoogleAuthManager: deep link ignored (no login in progress)";
    return;
  }
  if (!m_bridgeState.isEmpty() && m_bridgeState != state) {
    qWarning() << "GoogleAuthManager: deep-link state mismatch with in-flight — ignoring";
    return;
  }
  if (m_claimInFlight) {
    QTimer::singleShot(400, this, [this, state]() {
      if (m_loginInProgress && m_bridgeState == state)
        claimDesktopState(state, true);
    });
    return;
  }
  claimDesktopState(state, true);
}

void GoogleAuthManager::claimDesktopState(const QString &state, bool fromDeepLink) {
  if (state.isEmpty() || m_claimInFlight || m_desktopPkceActive)
    return;
  if (!m_networkManager)
    return;
  if (m_loginInProgress && !m_bridgeState.isEmpty() && m_bridgeState != state) {
    qWarning() << "GoogleAuthManager: claim skipped (state mismatch)";
    return;
  }

  m_claimInFlight = true;
  QUrl url(QString::fromLatin1(kDesktopClaimUrl));
  QUrlQuery q;
  q.addQueryItem(QStringLiteral("state"), state);
  url.setQuery(q);

  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("BlopDesktop/GoogleAuthClaim"));
  QNetworkReply *reply = m_networkManager->get(req);
  connect(reply, &QNetworkReply::finished, this,
          [this, reply, fromDeepLink, state]() {
            reply->deleteLater();
            m_claimInFlight = false;
            if (!m_loginInProgress || m_desktopPkceActive)
              return;
            if (!m_bridgeState.isEmpty() && m_bridgeState != state)
              return;

            const auto netErr = reply->error();
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                    .toInt();
            const QByteArray raw = reply->readAll();
            if (netErr != QNetworkReply::NoError || status >= 400) {
              qWarning() << "GoogleAuthManager: claim failed status=" << status
                         << "err=" << reply->errorString()
                         << "deepLink=" << fromDeepLink;
              if (fromDeepLink && m_loginInProgress) {
                if (m_bridgePoll && !m_bridgePoll->isActive())
                  m_bridgePoll->start();
              }
              return;
            }

            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            if (!doc.isObject())
              return;
            const QJsonObject obj = doc.object();
            if (!obj.value(QStringLiteral("ready")).toBool(false)) {
              if (fromDeepLink && m_loginInProgress && m_bridgePoll &&
                  !m_bridgePoll->isActive())
                m_bridgePoll->start();
              return;
            }

            const QString credential =
                obj.value(QStringLiteral("credential")).toString();
            const QString accessTok =
                obj.value(QStringLiteral("access_token")).toString();
            if (!accessTok.isEmpty()) {
              persistAccessToken(accessTok);
              qInfo() << "GoogleAuthManager: desktop claim includes calendar access_token";
            }
            if (credential.isEmpty()) {
              finishDesktopBridge(QString(),
                                  QStringLiteral("oauth_missing_credential"));
              return;
            }
            finishDesktopBridge(credential, QString());
          });
}

void GoogleAuthManager::finishDesktopBridge(const QString &idToken,
                                            const QString &error) {
  stopDesktopBridgeTimers();
  m_loginInProgress = false;
  m_loginInProgressSinceMs = 0;
  m_bridgeState.clear();
  m_claimInFlight = false;
  m_wantCalendar = false;

  if (!error.isEmpty() || idToken.isEmpty()) {
    qWarning() << "GoogleAuthManager: desktop bridge failed:" << error;
    emit authenticationFailed(error.isEmpty()
                                  ? QStringLiteral("oauth_bridge_failed")
                                  : error);
    return;
  }

  qInfo() << "GoogleAuthManager: desktop bridge received id_token via claim"
          << "calendarAccess=" << hasCalendarAccess();
  parseUserInfoFromIdToken(idToken);
  m_authenticated = true;
  emit idTokenReceived(idToken);
  emit authenticated();
}
#endif
