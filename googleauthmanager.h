#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#ifndef Q_OS_ANDROID
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#endif

class QNetworkAccessManager;

class GoogleAuthManager : public QObject {
    Q_OBJECT
public:
    static GoogleAuthManager& instance();

    void login();
    bool isAuthenticated() const { return m_authenticated; }

    QString userEmail() const { return m_email; }
    QString userName() const { return m_name; }
    QString userPictureUrl() const { return m_pictureUrl; }

#ifdef Q_OS_ANDROID
    /// Called from JNI bridge (BlopOAuthBridge) when Android delivers the
    /// custom-scheme deep link with the OAuth response.
    void handleDeepLinkCallback(const QString &uri);
#endif

signals:
    void authenticated();
    void authenticationFailed(const QString& error);
    void userInfoUpdated();
    void idTokenReceived(const QString& idToken);
    void requireBrowser(const QUrl &url);

private:
    explicit GoogleAuthManager(QObject* parent = nullptr);
    ~GoogleAuthManager() = default;

    GoogleAuthManager(const GoogleAuthManager&) = delete;
    GoogleAuthManager& operator=(const GoogleAuthManager&) = delete;

#ifdef Q_OS_ANDROID
    void startPkceLogin();
    void exchangeAuthorizationCode(const QString &code);
    void parseUserInfoFromIdToken(const QString &idToken);
    static QString generateRandomString(int length);
    static QString base64UrlEncode(const QByteArray &data);

    QNetworkAccessManager *m_networkManager{nullptr};
    QString m_pkceVerifier;
    QString m_pkceState;
    QString m_redirectUri;
    QString m_clientId;
    bool m_loginInProgress{false};
#else
    void fetchUserInfo();
    QOAuth2AuthorizationCodeFlow* m_oauth2{nullptr};
    QOAuthHttpServerReplyHandler* m_replyHandler{nullptr};
#endif

    QString m_email;
    QString m_name;
    QString m_pictureUrl;
    bool m_authenticated{false};
};
