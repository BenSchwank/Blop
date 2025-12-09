#include "UiProfileManager.h"
#include <QJsonDocument>
#include <QJsonArray>

void UiProfileManager::saveProfiles() {
    QSettings settings("BenSchwank", "Blop");
    QJsonArray arr;
    for(const auto& p : m_profiles) arr.append(p.toJson());
    settings.setValue("uiProfiles", QJsonDocument(arr).toJson(QJsonDocument::Compact));
    settings.setValue("lastActiveProfileId", m_currentProfile.id);
}

void UiProfileManager::loadProfiles() {
    QSettings settings("BenSchwank", "Blop");
    QByteArray data = settings.value("uiProfiles").toByteArray();

    m_profiles.clear();
    if (!data.isEmpty()) {
        QJsonArray arr = QJsonDocument::fromJson(data).array();
        for(const auto& val : arr) {
            m_profiles.append(UiProfile::fromJson(val.toObject()));
        }
    }

    if (m_profiles.isEmpty()) {
        m_profiles.append(UiProfile::createDesktop());
        m_profiles.append(UiProfile::createTablet());
    }

    QString lastId = settings.value("lastActiveProfileId").toString();
    bool found = false;
    for(const auto& p : m_profiles) {
        if(p.id == lastId) {
            m_currentProfile = p;
            found = true;
            break;
        }
    }
    if(!found) m_currentProfile = m_profiles.first();
}
