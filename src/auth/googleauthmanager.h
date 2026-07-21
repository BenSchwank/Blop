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
    /// Clear the in-progress PKCE lock so the next login() tap triggers a
    /// fresh flow. Called when the browser could not be opened, the user
    /// returned without a redirect, or MainWindow cancels the wait.
    void cancelPendingLogin();
    bool isLoginInProgress() const { return m_loginInProgress; }
#endif

    QString userEmail() const { return m_email; }
    QString userName() const { return m_name; }
    QString userPictureUrl() const { return m_pictureUrl; }

#ifdef Q_OS_ANDROID
    /// Called from JNI bridge (BlopOAuthBridge) when Android delivers the
    /// custom-scheme deep link with the OAuth response.
    void handleDeepLinkCallback(const QString &uri);
    /// Activity resumed after Custom Tab; deep link may still arrive shortly.
    void handleExternalAuthResume();
    /// Browser handoff failed before any redirect.
    void handleExternalAuthAbandoned(const QString &reason);
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
    /// Timestamp of the most recent startPkceLogin(). If the browser fails
    /// to open and `m_loginInProgress` is never cleared by a callback,
    /// subsequent login() calls would be ignored forever. We treat a lock
    /// older than 10 minutes as stale (user may spend minutes in Google
    /// account picker / 2FA) and let the next tap restart the flow.
    qint64 m_loginInProgressSinceMs{0};
    /// Serial for resume-grace timers so a late deep link wins over abandon.
    int m_authResumeGeneration{0};
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
