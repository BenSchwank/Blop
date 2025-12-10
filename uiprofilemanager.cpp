#include "UiProfileManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>

UiProfileManager::UiProfileManager(QObject *parent) : QObject(parent) {
    loadProfiles();
    if (m_profiles.isEmpty()) {
        ensureDefaults();
    }
}

QString UiProfileManager::getFilePath() const {
#ifdef Q_OS_ANDROID
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
#endif
    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");
    return path + "/uiprofiles.json";
}

void UiProfileManager::ensureDefaults() {
    UiProfile desktop;
    desktop.id = "{desktop-default}";
    desktop.name = "Desktop (Kompakt)";
    desktop.iconSize = 100;
    desktop.gridSpacing = 10;
    desktop.toolbarScale = 1.0;
    desktop.buttonSize = 32;
    m_profiles.append(desktop);

    UiProfile tablet;
    tablet.id = "{tablet-default}";
    tablet.name = "Tablet (Groß)";
    tablet.iconSize = 160;
    tablet.gridSpacing = 30;
    tablet.toolbarScale = 1.3;
    tablet.buttonSize = 56;
    m_profiles.append(tablet);

    m_currentId = desktop.id;
    saveProfiles();
    emit listChanged();
}

void UiProfileManager::loadProfiles() {
    QFile file(getFilePath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    m_currentId = root.value("currentId").toString();
    QJsonArray arr = root.value("profiles").toArray();

    m_profiles.clear();
    for (const auto &val : arr) {
        m_profiles.append(UiProfile::fromJson(val.toObject()));
    }

    if (!m_currentId.isEmpty()) {
        emit profileChanged(currentProfile());
    }
}

void UiProfileManager::saveProfiles() {
    QJsonArray arr;
    for (const auto &p : m_profiles) {
        arr.append(p.toJson());
    }
    QJsonObject root;
    root["currentId"] = m_currentId;
    root["profiles"] = arr;

    QFile file(getFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
}

UiProfile UiProfileManager::profileById(const QString &id) const {
    for (const auto &p : m_profiles) {
        if (p.id == id) return p;
    }
    return UiProfile();
}

UiProfile UiProfileManager::currentProfile() const {
    return profileById(m_currentId);
}

void UiProfileManager::setCurrentProfile(const QString &id) {
    if (m_currentId != id) {
        m_currentId = id;
        saveProfiles();
        emit profileChanged(currentProfile());
    }
}

void UiProfileManager::createProfile(const QString &name) {
    UiProfile p;
    p.name = name;
    UiProfile current = currentProfile();
    if (!current.id.isEmpty()) {
        p.iconSize = current.iconSize;
        p.gridSpacing = current.gridSpacing;
        p.toolbarScale = current.toolbarScale;
        p.buttonSize = current.buttonSize;
        p.snapToGrid = current.snapToGrid;
    }
    m_profiles.append(p);
    saveProfiles();
    emit listChanged();
}

void UiProfileManager::updateProfile(const UiProfile &profile, bool saveToDisk) {
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == profile.id) {
            bool nameChanged = (m_profiles[i].name != profile.name);
            m_profiles[i] = profile;

            if (saveToDisk) saveProfiles();
            if (nameChanged) emit listChanged();
            if (m_currentId == profile.id) emit profileChanged(profile);
            return;
        }
    }
}

void UiProfileManager::deleteProfile(const QString &id) {
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles[i].id == id) {
            m_profiles.removeAt(i);
            if (m_currentId == id && !m_profiles.isEmpty()) {
                setCurrentProfile(m_profiles.first().id);
            }
            saveProfiles();
            emit listChanged();
            return;
        }
    }
}
