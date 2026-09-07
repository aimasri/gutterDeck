#include "ConfigManager.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <algorithm>

ConfigManager::ConfigManager(const QString& customConfigPath)
    : m_customConfigPath(customConfigPath) {}

QString ConfigManager::getConfigDirPath() const {
    if (!m_customConfigPath.isEmpty()) {
        return QFileInfo(m_customConfigPath).dir().absolutePath();
    }
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck";
}

QString ConfigManager::getConfigFilePath() const {
    if (!m_customConfigPath.isEmpty()) {
        return m_customConfigPath;
    }
    return getConfigDirPath() + "/config.json";
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
    m_settings.targetScreen = "auto";
    m_settings.targetWorkspace = -1;
}

void ConfigManager::validate() {
    m_settings.gutterWidth = std::clamp(m_settings.gutterWidth, 2, 100);
    m_settings.gutterExpandedWidth = std::clamp(m_settings.gutterExpandedWidth,
                                                m_settings.gutterWidth + 4, 300);
    m_settings.animationDurationMs = std::clamp(m_settings.animationDurationMs, 50, 2000);
    m_settings.swellDurationMs = std::clamp(m_settings.swellDurationMs, 50, 1000);
    if (m_settings.targetScreen.trimmed().isEmpty()) {
        m_settings.targetScreen = "auto";
    }

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
        m_settings.targetScreen = settingsObj.value("target_screen").toString("auto");
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
    if (!m_settings.targetScreen.isEmpty()) {
        settingsObj["target_screen"] = m_settings.targetScreen;
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
