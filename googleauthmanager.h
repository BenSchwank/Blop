#pragma once

#include <QObject>
#include <QString>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>

class GoogleAuthManager : public QObject {
    Q_OBJECT
public:
    static GoogleAuthManager& instance();

    void login();
    bool isAuthenticated() const { return m_authenticated; }
    
    QString userEmail() const { return m_email; }
    QString userName() const { return m_name; }
    QString userPictureUrl() const { return m_pictureUrl; }

signals:
    void authenticated();
    void authenticationFailed(const QString& error);
    void userInfoUpdated();

private:
    explicit GoogleAuthManager(QObject* parent = nullptr);
    ~GoogleAuthManager() = default;

    GoogleAuthManager(const GoogleAuthManager&) = delete;
    GoogleAuthManager& operator=(const GoogleAuthManager&) = delete;

    void fetchUserInfo();

    QOAuth2AuthorizationCodeFlow* m_oauth2;
    QOAuthHttpServerReplyHandler* m_replyHandler;

    QString m_email;
    QString m_name;
    QString m_pictureUrl;
    bool m_authenticated{false};
};
