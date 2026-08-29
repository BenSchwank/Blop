#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
#ifndef Q_OS_ANDROID
class QTimer;
class QTcpServer;
#endif

class GoogleAuthManager : public QObject {
    Q_OBJECT
public:
    static GoogleAuthManager& instance();

    void login();
    /// Request Google Calendar scopes. Android: PKCE custom-scheme.
    /// Desktop: loopback PKCE (Desktop OAuth client) so we get a real
    /// Calendar access_token without relying on the GIS bridge deploy.
    void loginForCalendar();
    bool isAuthenticated() const { return m_authenticated; }
    bool hasCalendarAccess() const;
    QString accessToken() const;
    void clearCalendarAccess();

    QString userEmail() const { return m_email; }
    QString userName() const { return m_name; }
    QString userPictureUrl() const { return m_pictureUrl; }

#ifdef Q_OS_ANDROID
    /// Clear the in-progress PKCE lock so the next login() tap triggers a
    /// fresh flow. Called when the browser could not be opened, the user
    /// returned without a redirect, or MainWindow cancels the wait.
    void cancelPendingLogin();
    bool isLoginInProgress() const { return m_loginInProgress; }
    /// Called from JNI bridge (BlopOAuthBridge) when Android delivers the
    /// custom-scheme deep link with the OAuth response.
    void handleDeepLinkCallback(const QString &uri);
    /// Activity resumed after Custom Tab; deep link may still arrive shortly.
    void handleExternalAuthResume();
    /// Browser handoff failed before any redirect.
    void handleExternalAuthAbandoned(const QString &reason);
#else
    /// Cancel an in-flight desktop bridge / loopback PKCE login.
    void cancelPendingLogin();
    bool isLoginInProgress() const { return m_loginInProgress; }
    /// Called when OS opens blop://oauth/done?state=... (GIS bridge return).
    void handleDesktopOAuthDeepLink(const QUrl &url);
#endif

signals:
    void authenticated();
    void authenticationFailed(const QString& error);
    void userInfoUpdated();
    void idTokenReceived(const QString& idToken);
    void requireBrowser(const QUrl &url);
    void calendarTokenUpdated();
#ifdef Q_OS_ANDROID
    /// Custom-scheme redirect arrived; token exchange / backend verify still running.
    void redirectReceived();
#endif

private:
    explicit GoogleAuthManager(QObject* parent = nullptr);
    ~GoogleAuthManager() = default;

    GoogleAuthManager(const GoogleAuthManager&) = delete;
    GoogleAuthManager& operator=(const GoogleAuthManager&) = delete;

    void parseUserInfoFromIdToken(const QString &idToken);
    void persistAccessToken(const QString &token);
    void loadPersistedAccessToken();

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
    /// Sign-in only: GIS bridge on blop-study.com (id_token via /claim).
    void startDesktopBridgeLogin();
    /// Calendar: Desktop OAuth client + loopback PKCE → access_token.
    void startDesktopCalendarPkceLogin();
    void stopDesktopLoopbackServer();
    void onDesktopLoopbackConnection();
    void exchangeDesktopAuthorizationCode(const QString &code);
    void pollDesktopClaim();
    void claimDesktopState(const QString &state, bool fromDeepLink);
    void finishDesktopBridge(const QString &idToken, const QString &error);
    void stopDesktopBridgeTimers();
    static QString generateRandomString(int length);
    static QString base64UrlEncode(const QByteArray &data);

    QNetworkAccessManager *m_networkManager{nullptr};
    QTimer *m_bridgeTimeout{nullptr};
    QTimer *m_bridgePoll{nullptr};
    QTcpServer *m_loopbackServer{nullptr};
    QString m_bridgeState;
    QString m_lastHandledDeepLinkState;
    qint64 m_lastHandledDeepLinkMs{0};
    QString m_pkceVerifier;
    QString m_pkceState;
    QString m_redirectUri;
    QString m_clientId;
    bool m_loginInProgress{false};
    qint64 m_loginInProgressSinceMs{0};
    bool m_claimInFlight{false};
    bool m_desktopPkceActive{false};
#endif

    QString m_email;
    QString m_name;
    QString m_pictureUrl;
    QString m_accessToken;
    bool m_authenticated{false};
    bool m_wantCalendar{false};
};
