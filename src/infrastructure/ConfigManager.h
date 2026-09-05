#pragma once

#include <QColor>
#include <QString>
#include <QVector>

namespace ConfigDefaults {
    constexpr int GUTTER_WIDTH = 4;
    constexpr int GUTTER_EXPANDED_WIDTH = 44;
    constexpr int ANIMATION_DURATION_MS = 250;
    constexpr int SWELL_DURATION_MS = 160;
    constexpr int WATCHDOG_TIMEOUT_MS = 5000;
}

/**
 * @brief Represents configuration for a single deck slot attached to an application.
 */
struct DeckConfig {
    QString id;
    QString name;
    QString command;
    QColor color;
};

/**
 * @brief Represents global layout and visual presentation settings.
 */
struct AppSettings {
    int gutterWidth = ConfigDefaults::GUTTER_WIDTH;
    int gutterExpandedWidth = ConfigDefaults::GUTTER_EXPANDED_WIDTH;
    int animationDurationMs = ConfigDefaults::ANIMATION_DURATION_MS;
    int swellDurationMs = ConfigDefaults::SWELL_DURATION_MS;
    int screenWidth = 0;
    int screenHeight = 0;
    QString targetScreen = "auto";
    int targetWorkspace = -1;
};

/**
 * @brief Manages JSON configuration loading, serialization, and validation.
 * @details Reads and writes ~/.config/gutter-deck/config.json. Guarantees safe
 *          fallbacks, sane boundaries via validate(), and explicit persistence.
 * @note Decks are arbitrary executable commands chosen by the user.
 */
class ConfigManager {
public:
    explicit ConfigManager(const QString& customConfigPath = QString());
    ~ConfigManager() = default;

    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;

    /**
     * @brief Loads configuration from disk, creating defaults if missing.
     * @return True if loaded or defaulted successfully.
     */
    [[nodiscard]] bool loadConfig();

    /**
     * @brief Saves current configuration to disk.
     * @return True if file write succeeded.
     */
    [[nodiscard]] bool saveConfig() const;

    /**
     * @brief Returns immutable list of configured decks.
     */
    [[nodiscard]] const QVector<DeckConfig>& getDecks() const noexcept;

    /**
     * @brief Returns immutable application settings.
     */
    [[nodiscard]] const AppSettings& getSettings() const noexcept;

    /**
     * @brief Sets deck configurations and validates bounds.
     */
    void setDecks(const QVector<DeckConfig>& decks);

    /**
     * @brief Sets application settings and validates bounds.
     */
    void setSettings(const AppSettings& settings);

    /**
     * @brief Updates an existing deck slot and automatically saves to disk.
     * @return True if update and save succeeded.
     */
    bool updateDeck(int index, const QString& name, const QString& command, const QColor& color);

    /**
     * @brief Appends a new deck slot and automatically saves to disk.
     * @return True if addition and save succeeded.
     */
    bool addDeck(const DeckConfig& deck);

    /**
     * @brief Inserts a new deck slot at a specific index and automatically saves to disk.
     * @return True if insertion and save succeeded.
     */
    bool insertDeck(int index, const DeckConfig& deck);

    /**
     * @brief Removes a deck slot and automatically saves to disk.
     * @note Will refuse to remove if only 1 deck remains.
     * @return True if removal and save succeeded.
     */
    bool removeDeck(int index);

    /**
     * @brief Reorders decks according to a permutation of indices and automatically saves to disk.
     * @param newOrder Array of original deck indices in their new sequence.
     * @return True if reordering and save succeeded.
     */
    bool reorderDecks(const QVector<int>& newOrder);

    /**
     * @brief Validates and sanitizes settings and deck entries to prevent out-of-bound errors.
     */
    void validate();

    /**
     * @brief Gets the full absolute path to the configuration file.
     */
    [[nodiscard]] QString getConfigFilePath() const;

    /**
     * @brief Gets the absolute directory path where configuration is stored.
     */
    [[nodiscard]] QString getConfigDirPath() const;

private:
    QString m_customConfigPath;
    QVector<DeckConfig> m_decks;
    AppSettings m_settings;

    void populateDefaultConfiguration();
    void populateDefaultDecks();
};
