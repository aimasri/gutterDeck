#include "ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QRegularExpression>
#include <algorithm>

QString ConfigManager::getProfilesRegistryPath() {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck/profiles.json";
}

QString ConfigManager::getProfileConfigPath(const QString& profileId) {
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck/profiles/" + profileId + "/config.json";
}

bool ConfigManager::profileExists(const QString& profileId) {
    auto profiles = listProfiles();
    for (const auto& p : profiles) {
        if (p.id == profileId) return true;
    }
    return false;
}

QVector<ProfileInfo> ConfigManager::listProfiles() {
    QVector<ProfileInfo> profiles;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject() && doc.object().contains("profiles")) {
            QJsonArray arr = doc.object()["profiles"].toArray();
            for (const auto& val : arr) {
                QJsonObject obj = val.toObject();
                ProfileInfo info;
                info.id = obj["id"].toString();
                info.displayName = obj["displayName"].toString();
                info.accentColor = QColor(obj["accentColor"].toString("#802563eb"));
                profiles.append(info);
            }
        }
    }
    return profiles;
}

QString ConfigManager::getAutoLaunchProfile() {
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isObject()) {
            return doc.object()["auto_launch"].toString();
        }
    }
    return QString();
}

void ConfigManager::setAutoLaunchProfile(const QString& profileId) {
    QFile file(getProfilesRegistryPath());
    QJsonObject root;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    if (profileId.isEmpty()) {
        root.remove("auto_launch");
    } else {
        root["auto_launch"] = profileId;
    }
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

bool ConfigManager::createProfile(const QString& displayName, const QColor& accentColor) {
    QString id = displayName.toLower().replace(QRegularExpression("[^a-z0-9]+"), "-");
    id = id.trimmed();
    if (id.endsWith("-")) id.chop(1);
    if (id.startsWith("-")) id.remove(0, 1);
    if (id.isEmpty()) id = "profile";

    QString originalId = id;
    int counter = 1;
    while (profileExists(id)) {
        id = QString("%1-%2").arg(originalId).arg(counter++);
    }

    auto profiles = listProfiles();
    ProfileInfo info{id, displayName, accentColor};
    profiles.append(info);

    QJsonObject root;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonArray arr;
    for (const auto& p : profiles) {
        QJsonObject obj;
        obj["id"] = p.id;
        obj["displayName"] = p.displayName;
        obj["accentColor"] = p.accentColor.name(QColor::HexArgb);
        arr.append(obj);
    }
    root["profiles"] = arr;

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        
        // Ensure directory exists and create a default config.json
        ConfigManager newConfig(id);
        (void)newConfig.loadConfig(); // will generate defaults and save
        return true;
    }
    return false;
}

bool ConfigManager::deleteProfile(const QString& profileId) {
    auto profiles = listProfiles();
    if (profiles.size() <= 1) return false; // Don't delete last profile

    bool found = false;
    QJsonArray arr;
    for (const auto& p : profiles) {
        if (p.id == profileId) {
            found = true;
        } else {
            QJsonObject obj;
            obj["id"] = p.id;
            obj["displayName"] = p.displayName;
            obj["accentColor"] = p.accentColor.name(QColor::HexArgb);
            arr.append(obj);
        }
    }

    if (!found) return false;

    QJsonObject root;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    root["profiles"] = arr;
    if (root["auto_launch"].toString() == profileId) {
        root.remove("auto_launch");
    }

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();

        // Delete profile directory
        QString dirPath = QFileInfo(getProfileConfigPath(profileId)).dir().absolutePath();
        QDir(dirPath).removeRecursively();
        return true;
    }
    return false;
}

bool ConfigManager::renameProfile(const QString& profileId, const QString& newDisplayName) {
    auto profiles = listProfiles();
    bool found = false;
    QJsonArray arr;
    for (auto& p : profiles) {
        if (p.id == profileId) {
            p.displayName = newDisplayName;
            found = true;
        }
        QJsonObject obj;
        obj["id"] = p.id;
        obj["displayName"] = p.displayName;
        obj["accentColor"] = p.accentColor.name(QColor::HexArgb);
        arr.append(obj);
    }

    if (!found) return false;

    QJsonObject root;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    root["profiles"] = arr;

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }
    return false;
}

bool ConfigManager::updateProfileColor(const QString& profileId, const QColor& newColor) {
    auto profiles = listProfiles();
    bool found = false;
    QJsonArray arr;
    for (auto& p : profiles) {
        if (p.id == profileId) {
            p.accentColor = newColor;
            found = true;
        }
        QJsonObject obj;
        obj["id"] = p.id;
        obj["displayName"] = p.displayName;
        obj["accentColor"] = p.accentColor.name(QColor::HexArgb);
        arr.append(obj);
    }

    if (!found) return false;

    QJsonObject root;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
    root["profiles"] = arr;

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }
    return false;
}

bool ConfigManager::reorderProfiles(const QVector<QString>& newProfileIds) {
    auto currentProfiles = listProfiles();
    if (currentProfiles.isEmpty() || newProfileIds.isEmpty()) {
        return false;
    }

    QVector<ProfileInfo> reordered;
    reordered.reserve(currentProfiles.size());

    // 1. Place profiles according to newProfileIds
    for (const QString& id : newProfileIds) {
        auto it = std::find_if(currentProfiles.begin(), currentProfiles.end(),
                               [&id](const ProfileInfo& p) { return p.id == id; });
        if (it != currentProfiles.end()) {
            reordered.append(*it);
        }
    }

    // 2. Safety: Append any profiles that might have been missing from newProfileIds
    for (const auto& p : currentProfiles) {
        if (!newProfileIds.contains(p.id)) {
            reordered.append(p);
        }
    }

    QJsonObject root;
    QFile file(getProfilesRegistryPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonArray arr;
    for (const auto& p : reordered) {
        QJsonObject obj;
        obj["id"] = p.id;
        obj["displayName"] = p.displayName;
        obj["accentColor"] = p.accentColor.name(QColor::HexArgb);
        arr.append(obj);
    }
    root["profiles"] = arr;

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        return true;
    }
    return false;
}

bool ConfigManager::moveProfile(const QString& profileId, int delta) {
    auto profiles = listProfiles();
    int idx = -1;
    for (int i = 0; i < profiles.size(); ++i) {
        if (profiles[i].id == profileId) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        return false;
    }

    int targetIdx = idx + delta;
    if (targetIdx < 0 || targetIdx >= profiles.size() || targetIdx == idx) {
        return false;
    }

    std::swap(profiles[idx], profiles[targetIdx]);

    QVector<QString> newIds;
    newIds.reserve(profiles.size());
    for (const auto& p : profiles) {
        newIds.append(p.id);
    }

    return reorderProfiles(newIds);
}

void ConfigManager::migrateIfNeeded() {
    QFile registryFile(getProfilesRegistryPath());
    if (registryFile.exists()) {
        return; // Already migrated or created
    }

    QString oldConfigPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck/config.json";
    QFile oldConfigFile(oldConfigPath);

    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck/profiles/default");

    if (oldConfigFile.exists()) {
        QString newConfigPath = getProfileConfigPath("default");
        oldConfigFile.copy(newConfigPath);
        oldConfigFile.rename(oldConfigPath + ".backup");
    }

    QJsonObject defaultProf;
    defaultProf["id"] = "default";
    defaultProf["displayName"] = "Default";
    defaultProf["accentColor"] = "#802563eb";
    
    QJsonArray arr;
    arr.append(defaultProf);
    
    QJsonObject root;
    root["profiles"] = arr;
    
    if (registryFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        registryFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

ConfigManager::ConfigManager(const QString& profileIdOrPath) {
    if (!profileIdOrPath.isEmpty()) {
        if (profileIdOrPath.contains('/') || profileIdOrPath.endsWith(".json", Qt::CaseInsensitive)) {
            m_customConfigPath = profileIdOrPath;
        } else {
            m_profileId = profileIdOrPath;
            m_customConfigPath = getProfileConfigPath(profileIdOrPath);
        }
    } else {
        // Fallback for tests or single-instance
        m_customConfigPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck/config.json";
    }
}

QString ConfigManager::getProfileId() const noexcept {
    return m_profileId;
}

QString ConfigManager::getConfigDirPath() const {
    return QFileInfo(m_customConfigPath).dir().absolutePath();
}

QString ConfigManager::getConfigFilePath() const {
    return m_customConfigPath;
}

void ConfigManager::populateDefaultDecks() {
    m_decks.clear();

    DeckConfig deck1;
    deck1.id = "gutter_1";
    deck1.name = "Browser";
    deck1.command = "google-chrome --new-window";
    deck1.color = QColor("#802563eb");
    m_decks.append(deck1);

    DeckConfig deck2;
    deck2.id = "gutter_2";
    deck2.name = "Terminal";
    deck2.command = "x-terminal-emulator";
    deck2.color = QColor("#8010b981");
    m_decks.append(deck2);
}

void ConfigManager::populateDefaultConfiguration() {
    populateDefaultDecks();
    m_settings.gutterWidth = ConfigDefaults::GUTTER_WIDTH;
    m_settings.gutterExpandedWidth = ConfigDefaults::GUTTER_EXPANDED_WIDTH;
    m_settings.animationDurationMs = ConfigDefaults::ANIMATION_DURATION_MS;
    m_settings.swellDurationMs = ConfigDefaults::SWELL_DURATION_MS;
    m_settings.targetWorkspace = -1;
}

void ConfigManager::validate() {
    m_settings.gutterWidth = std::clamp(m_settings.gutterWidth, 2, 100);
    m_settings.gutterExpandedWidth = std::clamp(m_settings.gutterExpandedWidth,
                                                m_settings.gutterWidth + 4, 300);
    m_settings.animationDurationMs = std::clamp(m_settings.animationDurationMs, 50, 2000);
    m_settings.swellDurationMs = std::clamp(m_settings.swellDurationMs, 50, 1000);

    if (m_decks.isEmpty()) {
        populateDefaultDecks();
    }

    int fallbackIndex = 1;
    for (auto& deck : m_decks) {
        if (deck.id.isEmpty()) {
            deck.id = QString("gutter_%1").arg(fallbackIndex);
        }
        if (deck.name.isEmpty()) {
            deck.name = QString("Deck %1").arg(fallbackIndex);
        }
        if (!deck.color.isValid()) {
            deck.color = QColor(128, 128, 128, 128);
        }
        ++fallbackIndex;
    }
}

bool ConfigManager::loadConfig() {
    QString filePath = getConfigFilePath();
    QFile file(filePath);

    if (!file.exists()) {
        populateDefaultConfiguration();
        validate();
        return saveConfig();
    }

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        populateDefaultConfiguration();
        validate();
        return false;
    }

    QByteArray rawData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        populateDefaultConfiguration();
        validate();
        return false;
    }

    QJsonObject root = doc.object();

    // Parse Settings
    if (root.contains("settings") && root["settings"].isObject()) {
        QJsonObject settingsObj = root["settings"].toObject();
        m_settings.gutterWidth = settingsObj.value("gutter_width")
            .toInt(ConfigDefaults::GUTTER_WIDTH);
        m_settings.gutterExpandedWidth = settingsObj.value("gutter_expanded_width")
            .toInt(ConfigDefaults::GUTTER_EXPANDED_WIDTH);
        m_settings.animationDurationMs = settingsObj.value("animation_duration_ms")
            .toInt(ConfigDefaults::ANIMATION_DURATION_MS);
        m_settings.swellDurationMs = settingsObj.value("swell_duration_ms")
            .toInt(ConfigDefaults::SWELL_DURATION_MS);
        m_settings.screenWidth = settingsObj.value("screen_width").toInt(0);
        m_settings.screenHeight = settingsObj.value("screen_height").toInt(0);
        m_settings.targetWorkspace = settingsObj.value("target_workspace").toInt(-1);
    }

    // Parse Decks (supporting both "decks" and legacy "gutters" keys)
    m_decks.clear();
    QJsonArray decksArr;
    if (root.contains("decks") && root["decks"].isArray()) {
        decksArr = root["decks"].toArray();
    } else if (root.contains("gutters") && root["gutters"].isArray()) {
        decksArr = root["gutters"].toArray();
    }

    for (const QJsonValue& val : decksArr) {
        if (val.isObject()) {
            QJsonObject dObj = val.toObject();
            DeckConfig deck;
            deck.id = dObj.value("id").toString();
            deck.name = dObj.value("name").toString();
            deck.command = dObj.value("command").toString();
            deck.color = QColor(dObj.value("color").toString("#80FFFFFF"));
            m_decks.append(deck);
        }
    }

    validate();
    return true;
}

bool ConfigManager::saveConfig() const {
    QDir dir(getConfigDirPath());
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            return false;
        }
    }

    QJsonObject root;

    // Serialize Settings
    QJsonObject settingsObj;
    settingsObj["gutter_width"] = m_settings.gutterWidth;
    settingsObj["gutter_expanded_width"] = m_settings.gutterExpandedWidth;
    settingsObj["animation_duration_ms"] = m_settings.animationDurationMs;
    settingsObj["swell_duration_ms"] = m_settings.swellDurationMs;
    if (m_settings.screenWidth > 0) {
        settingsObj["screen_width"] = m_settings.screenWidth;
    }
    if (m_settings.screenHeight > 0) {
        settingsObj["screen_height"] = m_settings.screenHeight;
    }
    if (m_settings.targetWorkspace >= 0) {
        settingsObj["target_workspace"] = m_settings.targetWorkspace;
    }
    root["settings"] = settingsObj;

    // Serialize Decks
    QJsonArray decksArr;
    for (const auto& deck : m_decks) {
        QJsonObject dObj;
        dObj["id"] = deck.id;
        dObj["name"] = deck.name;
        dObj["command"] = deck.command;
        dObj["color"] = deck.color.name(QColor::HexArgb);
        decksArr.append(dObj);
    }
    root["decks"] = decksArr;

    QFile file(getConfigFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

const QVector<DeckConfig>& ConfigManager::getDecks() const noexcept {
    return m_decks;
}

const AppSettings& ConfigManager::getSettings() const noexcept {
    return m_settings;
}

void ConfigManager::setDecks(const QVector<DeckConfig>& decks) {
    m_decks = decks;
    validate();
}

void ConfigManager::setSettings(const AppSettings& settings) {
    m_settings = settings;
    validate();
}

bool ConfigManager::updateDeck(int index, const QString& name, const QString& command, const QColor& color) {
    if (index < 0 || index >= m_decks.size()) {
        return false;
    }
    if (!name.trimmed().isEmpty()) {
        m_decks[index].name = name.trimmed();
    }
    if (!command.trimmed().isEmpty()) {
        m_decks[index].command = command.trimmed();
    }
    if (color.isValid()) {
        m_decks[index].color = color;
    }
    validate();
    return saveConfig();
}

bool ConfigManager::addDeck(const DeckConfig& deck) {
    DeckConfig newDeck = deck;
    if (newDeck.id.isEmpty()) {
        newDeck.id = QString("gutter_%1").arg(m_decks.size() + 1);
    }
    if (newDeck.name.isEmpty()) {
        newDeck.name = QString("Deck %1").arg(m_decks.size() + 1);
    }
    if (!newDeck.color.isValid()) {
        newDeck.color = QColor("#802563eb");
    }
    m_decks.append(newDeck);
    validate();
    return saveConfig();
}

bool ConfigManager::insertDeck(int index, const DeckConfig& deck) {
    DeckConfig newDeck = deck;
    if (newDeck.id.isEmpty()) {
        newDeck.id = QString("gutter_%1").arg(m_decks.size() + 1);
    }
    if (newDeck.name.isEmpty()) {
        newDeck.name = QString("Deck %1").arg(m_decks.size() + 1);
    }
    if (!newDeck.color.isValid()) {
        newDeck.color = QColor("#802563eb");
    }
    int clampedIndex = std::max(0, std::min(index, static_cast<int>(m_decks.size())));
    m_decks.insert(clampedIndex, newDeck);
    validate();
    return saveConfig();
}

bool ConfigManager::removeDeck(int index) {
    if (m_decks.size() <= 1 || index < 0 || index >= m_decks.size()) {
        return false;
    }
    m_decks.removeAt(index);
    validate();
    return saveConfig();
}

bool ConfigManager::reorderDecks(const QVector<int>& newOrder) {
    if (newOrder.size() != m_decks.size()) {
        return false;
    }
    QVector<bool> seen(m_decks.size(), false);
    for (int idx : newOrder) {
        if (idx < 0 || idx >= m_decks.size() || seen[idx]) {
            return false;
        }
        seen[idx] = true;
    }

    QVector<DeckConfig> reordered;
    reordered.reserve(m_decks.size());
    for (int idx : newOrder) {
        reordered.append(m_decks[idx]);
    }
    m_decks = std::move(reordered);
    validate();
    return saveConfig();
}
