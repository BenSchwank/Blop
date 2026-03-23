#include "googleauthmanager.h"
#include <QDesktopServices>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QDebug>

GoogleAuthManager& GoogleAuthManager::instance() {
    static GoogleAuthManager instance;
    return instance;
}

GoogleAuthManager::GoogleAuthManager(QObject* parent) 
    : QObject(parent), m_oauth2(new QOAuth2AuthorizationCodeFlow(this)) 
{
    // The ReplyHandler intercepts the redirect to localhost at the specific port
    m_replyHandler = new QOAuthHttpServerReplyHandler(8080, this);
    m_oauth2->setReplyHandler(m_replyHandler);
    
    // Google OAuth2 endpoints
    m_oauth2->setAuthorizationUrl(QUrl("https://accounts.google.com/o/oauth2/auth"));
    m_oauth2->setAccessTokenUrl(QUrl("https://oauth2.googleapis.com/token"));
    
    // IMPORTANT: Provide a valid Google OAuth 2.0 Client ID for Web/Desktop Apps here!
    // For Desktop, Redirect URI must be: http://127.0.0.1:8080/
    // Paste your Client ID and Secret from Google Cloud Console below:
    m_oauth2->setClientIdentifier("YOUR_CLIENT_ID_GOES_HERE");
    m_oauth2->setClientIdentifierSharedKey("YOUR_CLIENT_SECRET_GOES_HERE");
    
    // Scopes needed for basic profile info
    m_oauth2->setScope("email profile");

    // Open the browser when authorization is required
    connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, 
            &QDesktopServices::openUrl);

    // React when an access token is successfully granted
    connect(m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this]() {
        qDebug() << "Google OAuth granted successfully!";
        m_authenticated = true;
        emit authenticated();
        
        // Fetch the user information automatically
        fetchUserInfo();
    });
}

void GoogleAuthManager::login() {
    m_oauth2->grant();
}

void GoogleAuthManager::fetchUserInfo() {
    QUrl userInfoUrl("https://www.googleapis.com/oauth2/v2/userinfo");
    QNetworkReply* reply = m_oauth2->get(userInfoUrl);
    
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
