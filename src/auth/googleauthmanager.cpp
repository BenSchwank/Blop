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
#include <QDateTime>
#include <QRandomGenerator>
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
// Render-hosted GIS bridge on the authorized blop-study.com origin.
constexpr const char *kDesktopBridgeUrl =
    "https://www.blop-study.com/api/auth/google/desktop/bridge";
constexpr const char *kDesktopClaimUrl =
    "https://www.blop-study.com/api/auth/google/desktop/claim";
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
    qWarning() << "GoogleAuthManager: desktop bridge timed out";
    finishDesktopBridge(QString(), QStringLiteral("oauth_bridge_timeout"));
  });
  m_bridgePoll = new QTimer(this);
  m_bridgePoll->setInterval(1000);
  connect(m_bridgePoll, &QTimer::timeout, this,
          &GoogleAuthManager::pollDesktopClaim);
#endif
}

void GoogleAuthManager::login() {
#ifdef Q_OS_ANDROID
  startPkceLogin();
#else
  startDesktopBridgeLogin();
#endif
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
  q.addQueryItem("scope", "openid email profile");
  q.addQueryItem("code_challenge", codeChallenge);
  q.addQueryItem("code_challenge_method", "S256");
  q.addQueryItem("state", m_pkceState);
  q.addQueryItem("prompt", "select_account");
  authUrl.setQuery(q);

  qInfo() << "GoogleAuthManager: launching PKCE auth via Custom Tab"
          << "redirect=" << m_redirectUri;
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

  exchangeAuthorizationCode(code);
}

void GoogleAuthManager::exchangeAuthorizationCode(const QString &code) {
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
    if (idToken.isEmpty()) {
      qWarning() << "Google token response missing id_token";
      m_loginInProgress = false;
      clearPersistedPkce();
      emit authenticationFailed(QStringLiteral("token_exchange_no_id_token"));
      return;
    }

    clearPersistedPkce();
    m_loginInProgress = false;
    parseUserInfoFromIdToken(idToken);
    m_authenticated = true;
    emit idTokenReceived(idToken);
    emit authenticated();
  });
}
#else // Desktop
QString GoogleAuthManager::generateRandomString(int length) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  QString out;
  out.reserve(length);
  for (int i = 0; i < length; ++i) {
    const quint32 idx =
        QRandomGenerator::system()->bounded(quint32(sizeof(alphabet) - 1));
    out.append(QChar::fromLatin1(alphabet[idx]));
  }
  return out;
}

void GoogleAuthManager::cancelPendingLogin() {
  if (!m_loginInProgress)
    return;
  qInfo() << "GoogleAuthManager: cancelling desktop bridge login";
  stopDesktopBridgeTimers();
  m_loginInProgress = false;
  m_loginInProgressSinceMs = 0;
  m_bridgeState.clear();
  m_claimInFlight = false;
}

void GoogleAuthManager::stopDesktopBridgeTimers() {
  if (m_bridgeTimeout)
    m_bridgeTimeout->stop();
  if (m_bridgePoll)
    m_bridgePoll->stop();
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
  if (!m_loginInProgress || m_bridgeState.isEmpty())
    return;
  claimDesktopState(m_bridgeState, false);
}

void GoogleAuthManager::handleDesktopOAuthDeepLink(const QUrl &url) {
  // Expected: blop://oauth/done?state=...  (or error=... from the provider).
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
    // Also accept blop://oauth/done/?state= or path-form.
    QUrlQuery pathQuery(url.query());
    state = pathQuery.queryItemValue(QStringLiteral("state"));
  }
  if (state.isEmpty()) {
    qWarning() << "GoogleAuthManager: deep link missing state" << url;
    emit authenticationFailed(QStringLiteral("oauth_missing_state"));
    return;
  }
  qInfo() << "GoogleAuthManager: desktop OAuth deep link state=" << state;
  // Poll may already have finished login — deep link is then only for focus.
  if (m_authenticated && !m_loginInProgress)
    return;
  // Ensure we treat this as an in-progress login so finish emits signals.
  if (!m_loginInProgress) {
    m_loginInProgress = true;
    m_loginInProgressSinceMs = QDateTime::currentMSecsSinceEpoch();
    m_bridgeState = state;
    if (m_bridgeTimeout)
      m_bridgeTimeout->start(5 * 60 * 1000);
    if (m_bridgePoll && !m_bridgePoll->isActive())
      m_bridgePoll->start();
  } else if (!m_bridgeState.isEmpty() && m_bridgeState != state) {
    qWarning() << "GoogleAuthManager: deep-link state mismatch with in-flight";
  }
  if (m_claimInFlight) {
    QTimer::singleShot(400, this, [this, state]() {
      claimDesktopState(state, true);
    });
    return;
  }
  claimDesktopState(state, true);
}

void GoogleAuthManager::claimDesktopState(const QString &state, bool fromDeepLink) {
  if (state.isEmpty() || m_claimInFlight)
    return;
  if (!m_networkManager)
    return;

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
          [this, reply, fromDeepLink]() {
            reply->deleteLater();
            m_claimInFlight = false;
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
                // Keep polling a bit — complete may still be in flight.
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

  if (!error.isEmpty() || idToken.isEmpty()) {
    qWarning() << "GoogleAuthManager: desktop bridge failed:" << error;
    emit authenticationFailed(error.isEmpty()
                                  ? QStringLiteral("oauth_bridge_failed")
                                  : error);
    return;
  }

  qInfo() << "GoogleAuthManager: desktop bridge received id_token via claim";
  parseUserInfoFromIdToken(idToken);
  m_authenticated = true;
  emit idTokenReceived(idToken);
  emit authenticated();
}
#endif
