#include "googleauthmanager.h"
#include <QDebug>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrl>

GoogleAuthManager &GoogleAuthManager::instance() {
  static GoogleAuthManager instance;
  return instance;
}

GoogleAuthManager::GoogleAuthManager(QObject *parent)
    : QObject(parent), m_oauth2(new QOAuth2AuthorizationCodeFlow(this)) {
  // The ReplyHandler intercepts the redirect to localhost at the specific port
  m_replyHandler = new QOAuthHttpServerReplyHandler(8080, this);
  m_oauth2->setReplyHandler(m_replyHandler);

  // Google OAuth2 endpoints
  m_oauth2->setAuthorizationUrl(
      QUrl("https://accounts.google.com/o/oauth2/auth"));
  m_oauth2->setAccessTokenUrl(QUrl("https://oauth2.googleapis.com/token"));

  // IMPORTANT: Provide a valid Google OAuth 2.0 Client ID for Web/Desktop Apps
  // here! For Desktop, Redirect URI must be: http://127.0.0.1:8080/ Paste your
  // Client ID and Secret from Google Cloud Console below:
  m_oauth2->setClientIdentifier(
      "571766217-omvcb33l9m0kr1bjk9ecdik6gcljpkf6.apps.googleusercontent.com");
  m_oauth2->setClientIdentifierSharedKey("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");

  // Scopes needed for basic profile info AND Google ID Token
  m_oauth2->setScope("openid email profile");

  // Request a browser (overlay) when authorization is required
  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser,
          this, &GoogleAuthManager::requireBrowser);

  // --- Error Logging ---
  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::error, this, [this](const QString &error, const QString &description, const QUrl &url) {
      qWarning() << "Google OAuth Error:" << error << description << url;
      emit authenticationFailed("OAuth Error: " + error + " - " + description);
  });
  
  // React when an access token is successfully granted
  connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this]() {
    qDebug() << "Google OAuth granted successfully!";
    m_authenticated = true;
    
    // Since Qt 6 QOAuth2AuthorizationCodeFlow doesn't reliably expose the id_token
    // from the token endpoint, we just pass the Access Token instead.
    // The backend handleGoogleLoginSuccess is updated to accept both JWT and Access Tokens.
    QString accessToken = m_oauth2->token();
    qDebug() << "Emitting Access Token to WebView bridge. Is empty?" << accessToken.isEmpty();
    
    if (accessToken.isEmpty()) {
        emit authenticationFailed("Google hat kein gültiges Access-Token gesendet!");
    } else {
        emit idTokenReceived(accessToken);
    }
    
    emit authenticated();

    // Fetch the user information automatically
    fetchUserInfo();
  });
}

void GoogleAuthManager::login() { m_oauth2->grant(); }

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
