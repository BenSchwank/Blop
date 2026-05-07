#include "googleauthmanager.h"

#include <QDebug>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_ANDROID
#include <QByteArray>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QtCore/qnativeinterface.h>
#include <QJniEnvironment>
#include <QJniObject>
#endif

namespace {
#ifdef Q_OS_ANDROID
// Android OAuth client (no client secret). Configure as "Android" client in
// Google Cloud Console with the package name + Play App Signing SHA-1 + upload
// SHA-1 fingerprints. Custom scheme matches the AndroidManifest intent filter.
constexpr const char *kAndroidClientId =
    "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com";
constexpr const char *kAndroidRedirectUri = "com.benschwank.blop:/oauth2redirect";
constexpr const char *kGoogleAuthEndpoint =
    "https://accounts.google.com/o/oauth2/v2/auth";
constexpr const char *kGoogleTokenEndpoint =
    "https://oauth2.googleapis.com/token";

// Bridge from BlopOAuthBridge.notifyAuthCallback (Java) into the Qt singleton.
// Registered via JNI in startPkceLogin() so the symbol is resolved before the
// custom tab redirects back to the app.
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
#endif // Q_OS_ANDROID
} // namespace

GoogleAuthManager &GoogleAuthManager::instance() {
  static GoogleAuthManager instance;
  return instance;
}

GoogleAuthManager::GoogleAuthManager(QObject *parent)
    : QObject(parent)
#ifndef Q_OS_ANDROID
      ,
      m_oauth2(new QOAuth2AuthorizationCodeFlow(this))
#endif
{
#ifdef Q_OS_ANDROID
  m_networkManager = new QNetworkAccessManager(this);
  m_clientId = QString::fromLatin1(kAndroidClientId);
  m_redirectUri = QString::fromLatin1(kAndroidRedirectUri);

  // Register the native callback so Java BlopOAuthBridge can call into us once
  // the deep link arrives.
  static bool s_jniRegistered = false;
  if (!s_jniRegistered) {
    QJniEnvironment env;
    JNINativeMethod methods[] = {{
        "nativeNotifyAuthCallback",
        "(Ljava/lang/String;)V",
        reinterpret_cast<void *>(
            Java_com_benschwank_blop_BlopOAuthBridge_nativeNotifyAuthCallback)}};
    if (env.registerNativeMethods("com/benschwank/blop/BlopOAuthBridge",
                                  methods, 1)) {
      s_jniRegistered = true;
      qInfo() << "GoogleAuthManager: registered BlopOAuthBridge JNI methods";
    } else {
      qWarning() << "GoogleAuthManager: failed to register BlopOAuthBridge JNI"
                    " methods (deep-link callback will not work)";
    }
  }

  // Drain any auth URI that arrived before the singleton was fully constructed.
  QJniObject pending = QJniObject::callStaticObjectMethod(
      "com/benschwank/blop/BlopOAuthBridge", "consumePendingUri",
      "()Ljava/lang/String;");
  if (pending.isValid()) {
    const QString s = pending.toString();
    if (!s.isEmpty())
      handleDeepLinkCallback(s);
  }
#else
  // Desktop: keep loopback flow.
  m_replyHandler = new QOAuthHttpServerReplyHandler(8080, this);
  m_oauth2->setReplyHandler(m_replyHandler);
  m_oauth2->setAuthorizationUrl(
      QUrl("https://accounts.google.com/o/oauth2/auth"));
  m_oauth2->setAccessTokenUrl(QUrl("https://oauth2.googleapis.com/token"));
  m_oauth2->setClientIdentifier(
      "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com");
  m_oauth2->setClientIdentifierSharedKey(
      "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");
  m_oauth2->setScope("openid email profile");

  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this,
          &GoogleAuthManager::requireBrowser);

  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::error, this,
          [this](const QString &error, const QString &description,
                 const QUrl &url) {
            qWarning() << "Google OAuth Error:" << error << description << url;
            emit authenticationFailed("OAuth Error: " + error + " - " +
                                      description);
          });

  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this]() {
    qDebug() << "Google OAuth granted successfully!";
    m_authenticated = true;
    QString accessToken = m_oauth2->token();
    if (accessToken.isEmpty())
      emit authenticationFailed(
          "Google hat kein gültiges Access-Token gesendet!");
    else
      emit idTokenReceived(accessToken);
    emit authenticated();
    fetchUserInfo();
  });
#endif
}

void GoogleAuthManager::login() {
#ifdef Q_OS_ANDROID
  startPkceLogin();
#else
  m_oauth2->grant();
#endif
}

#ifdef Q_OS_ANDROID
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
  if (m_loginInProgress) {
    qInfo() << "GoogleAuthManager: PKCE login already in progress";
    return;
  }
  m_loginInProgress = true;

  // PKCE: generate a high-entropy code_verifier and S256-hashed challenge.
  m_pkceVerifier = generateRandomString(64);
  const QByteArray challenge = QCryptographicHash::hash(
      m_pkceVerifier.toUtf8(), QCryptographicHash::Sha256);
  const QString codeChallenge = base64UrlEncode(challenge);
  m_pkceState = generateRandomString(32);

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

  qInfo() << "GoogleAuthManager: launching PKCE auth via Custom Tab";
  emit requireBrowser(authUrl);
}

void GoogleAuthManager::handleDeepLinkCallback(const QString &uri) {
  qInfo() << "GoogleAuthManager: deep-link callback received";
  m_loginInProgress = false;
  if (uri.isEmpty()) {
    emit authenticationFailed(QStringLiteral("empty_callback_uri"));
    return;
  }

  const QUrl parsed(uri);
  const QUrlQuery q(parsed);
  const QString error = q.queryItemValue("error");
  if (!error.isEmpty()) {
    qWarning() << "OAuth provider returned error:" << error;
    emit authenticationFailed(QStringLiteral("oauth_error:") + error);
    return;
  }

  const QString state = q.queryItemValue("state");
  if (state.isEmpty() || state != m_pkceState) {
    qWarning() << "OAuth state mismatch";
    emit authenticationFailed(QStringLiteral("oauth_state_mismatch"));
    return;
  }

  const QString code = q.queryItemValue("code");
  if (code.isEmpty()) {
    qWarning() << "OAuth callback missing code";
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
      emit authenticationFailed(QStringLiteral("token_exchange_failed"));
      return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject()) {
      qWarning() << "Google token response not JSON object:" << raw;
      emit authenticationFailed(QStringLiteral("token_exchange_invalid_json"));
      return;
    }
    const QJsonObject obj = doc.object();
    const QString idToken = obj.value("id_token").toString();
    if (idToken.isEmpty()) {
      qWarning() << "Google token response missing id_token";
      emit authenticationFailed(QStringLiteral("token_exchange_no_id_token"));
      return;
    }

    parseUserInfoFromIdToken(idToken);
    m_authenticated = true;
    emit idTokenReceived(idToken);
    emit authenticated();
  });
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
#else // Desktop only
void GoogleAuthManager::fetchUserInfo() {
  QUrl userInfoUrl("https://www.googleapis.com/oauth2/v2/userinfo");
  QNetworkReply *reply = m_oauth2->get(userInfoUrl);

  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (reply->error() == QNetworkReply::NoError) {
      QByteArray data = reply->readAll();
      QJsonDocument doc = QJsonDocument::fromJson(data);
      if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        m_email = obj.value("email").toString();
        m_name = obj.value("name").toString();
        m_pictureUrl = obj.value("picture").toString();
        emit userInfoUpdated();
      }
    } else {
      qWarning() << "Failed to fetch user info:" << reply->errorString();
      emit authenticationFailed(reply->errorString());
    }
    reply->deleteLater();
  });
}
#endif
