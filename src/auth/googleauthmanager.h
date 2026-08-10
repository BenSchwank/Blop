#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
#ifndef Q_OS_ANDROID
class QTimer;
#endif

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
#else
    /// Cancel an in-flight desktop bridge login (poll + timeout).
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

    void parseUserInfoFromIdToken(const QString &idToken);

#ifdef Q_OS_ANDROID
    void startPkceLogin();
    void exchangeAuthorizationCode(const QString &code);
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
    /// Desktop: open system browser to blop-study.com GIS bridge, then poll
    /// /api/auth/google/desktop/claim (no localhost redirect — Chrome blocks
    /// https→127.0.0.1 / Private Network Access).
    void startDesktopBridgeLogin();
    void pollDesktopClaim();
    void finishDesktopBridge(const QString &idToken, const QString &error);
    void stopDesktopBridgeTimers();
    static QString generateRandomString(int length);

    QNetworkAccessManager *m_networkManager{nullptr};
    QTimer *m_bridgeTimeout{nullptr};
    QTimer *m_bridgePoll{nullptr};
    QString m_bridgeState;
    bool m_loginInProgress{false};
    qint64 m_loginInProgressSinceMs{0};
    bool m_claimInFlight{false};
#endif

    QString m_email;
    QString m_name;
    QString m_pictureUrl;
    bool m_authenticated{false};
};
