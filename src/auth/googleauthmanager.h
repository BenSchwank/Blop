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

#ifdef Q_OS_ANDROID
    /// v3.18.6: clear the in-progress PKCE lock so the next login() tap
    /// triggers a fresh flow. Called by MainWindow when the browser
    /// could not be opened (otherwise the lock would only auto-clear
    /// after the 60 s stale-timeout in startPkceLogin()).
    void cancelPendingLogin();
#endif

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
    /// v3.18.6: timestamp of the most recent startPkceLogin(). If the
    /// browser fails to open and `m_loginInProgress` is never cleared by
    /// a callback, subsequent login() calls would be ignored forever.
    /// We treat a lock older than 60 s as stale and let the next tap
    /// restart the flow.
    qint64 m_loginInProgressSinceMs{0};
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
