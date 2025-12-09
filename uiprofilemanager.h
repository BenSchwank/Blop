#pragma once
#include "UiProfile.h"
#include <QList>
#include <QObject>
#include <QSettings>
#include <QUuid>

class UiProfileManager : public QObject {
    Q_OBJECT
public:
    static UiProfileManager& instance() {
        static UiProfileManager inst;
        return inst;
    }

    QList<UiProfile> profiles() const { return m_profiles; }
    UiProfile currentProfile() const { return m_currentProfile; }

    void addProfile(const QString& name) {
        UiProfile p = m_currentProfile; // Kopiere aktuelle Einstellungen als Basis
        p.id = QUuid::createUuid().toString();
        p.name = name;
        m_profiles.append(p);
        saveProfiles();
        emit profilesChanged();
    }

    void updateProfile(const UiProfile& p) {
        for(int i=0; i<m_profiles.size(); ++i) {
            if (m_profiles[i].id == p.id) {
                m_profiles[i] = p;
                if (m_currentProfile.id == p.id) {
                    m_currentProfile = p;
                    emit activeProfileChanged(p);
                }
                saveProfiles();
                emit profilesChanged();
                return;
            }
        }
    }

    void removeProfile(const QString& id) {
        if (m_profiles.size() <= 1) return; // Mindestens eins behalten
        for(int i=0; i<m_profiles.size(); ++i) {
            if (m_profiles[i].id == id) {
                m_profiles.removeAt(i);
                saveProfiles();
                emit profilesChanged();
                // Fallback wenn aktives gelöscht
                if (m_currentProfile.id == id) activateProfile(m_profiles.first().id);
                return;
            }
        }
    }

    void activateProfile(const QString& id) {
        for(const auto& p : m_profiles) {
            if (p.id == id) {
                m_currentProfile = p;
                saveProfiles(); // Merken welches aktiv ist
                emit activeProfileChanged(p);
                return;
            }
        }
    }

signals:
    void profilesChanged();
    void activeProfileChanged(UiProfile profile);

private:
    UiProfileManager() { loadProfiles(); }
    QList<UiProfile> m_profiles;
    UiProfile m_currentProfile;

    void saveProfiles();
    void loadProfiles();
};
