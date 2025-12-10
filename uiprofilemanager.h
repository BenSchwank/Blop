#pragma once
#include <QObject>
#include <QVector>
#include "UiProfile.h"

class UiProfileManager : public QObject {
    Q_OBJECT
public:
    explicit UiProfileManager(QObject *parent = nullptr);

    void loadProfiles();
    void saveProfiles();

    QVector<UiProfile> profiles() const { return m_profiles; }
    UiProfile profileById(const QString &id) const;
    UiProfile currentProfile() const;

    void createProfile(const QString &name);

    // Optimiert: Standardmäßig speichern, aber abschaltbar für Live-Vorschau
    void updateProfile(const UiProfile &profile, bool saveToDisk = true);

    void deleteProfile(const QString &id);
    void setCurrentProfile(const QString &id);

    void ensureDefaults();

signals:
    void profileChanged(UiProfile newProfile);
    void listChanged();

private:
    QVector<UiProfile> m_profiles;
    QString m_currentId;
    QString getFilePath() const;
};
