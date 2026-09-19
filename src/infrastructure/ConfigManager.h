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
 * @details Deserialized directly from the profile's JSON configuration. Contains user-defined
 *          presentation metadata and executable command string.
 * @note Immutable while read from getDecks(); mutated through explicit ConfigManager modification methods.
 */
struct DeckConfig {
    QString id;
    QString name;
    QString command;
    QColor color;
};

/**
 * @brief Represents global layout and visual presentation settings.
 * @details Controls edge gutter resting and swell dimensions, animation wipe timing,
 *          and target monitor geometry constraints.
 * @note Sane defaults are provided via ConfigDefaults constants and enforced by validate().
 */
struct AppSettings {
    int gutterWidth = ConfigDefaults::GUTTER_WIDTH;
    int gutterExpandedWidth = ConfigDefaults::GUTTER_EXPANDED_WIDTH;
    int animationDurationMs = ConfigDefaults::ANIMATION_DURATION_MS;
    int swellDurationMs = ConfigDefaults::SWELL_DURATION_MS;
    int screenWidth = 0;
    int screenHeight = 0;
    int targetWorkspace = -1;
};

/**
 * @brief Metadata for a gutterDeck profile in the global registry.
 * @details Stores the slug identifier, human-readable display name, and accent color.
 *          Persisted in ~/.config/gutter-deck/profiles.json.
 * @note The profile ID maps to the directory ~/.config/gutter-deck/profiles/<id>/.
 */
struct ProfileInfo {
    QString id;
    QString displayName;
    QColor accentColor;
};

/**
 * @brief Manages JSON configuration loading, serialization, and validation.
 * @details Reads and writes ~/.config/gutter-deck/profiles/<id>/config.json. Guarantees safe
 *          fallbacks, sane boundaries via validate(), and explicit persistence.
 * @note Decks are arbitrary executable commands chosen by the user.
 */
class ConfigManager {
public:
    /**
     * @brief Lists all registered profiles from profiles.json in saved sequence.
     * @return QVector of ProfileInfo structs.
     */
    static QVector<ProfileInfo> listProfiles();

    /**
     * @brief Creates a new profile directory and registers it in profiles.json.
     * @param displayName Human-readable title for the profile.
     * @param accentColor Primary theme accent color.
     * @return True if creation and disk write succeeded.
     */
    static bool createProfile(const QString& displayName, const QColor& accentColor);

    /**
     * @brief Deletes a profile and recursively wipes its directory.
     * @param profileId Slug identifier of profile to delete.
     * @return True if deleted; false if target profile is the only remaining profile.
     */
    static bool deleteProfile(const QString& profileId);

    /**
     * @brief Renames the display title of an existing profile.
     * @param profileId Target profile identifier.
     * @param newDisplayName Updated display name.
     * @return True if updated and persisted.
     */
    static bool renameProfile(const QString& profileId, const QString& newDisplayName);

    /**
     * @brief Updates the accent color of an existing profile.
     * @param profileId Target profile identifier.
     * @param newColor Updated QColor.
     * @return True if updated and persisted.
     */
    static bool updateProfileColor(const QString& profileId, const QColor& newColor);

    /**
     * @brief Reorders the profile registry matching a given sequence of profile IDs.
     * @param newProfileIds Sequence of profile slug IDs.
     * @return True if successfully reordered and persisted.
     */
    static bool reorderProfiles(const QVector<QString>& newProfileIds);

    /**
     * @brief Moves a profile relative to its current index by a delta.
     * @param profileId Target profile identifier.
     * @param delta Position shift (-1 for left/up, +1 for right/down).
     * @return True if moved and persisted; false if out of bounds.
     */
    static bool moveProfile(const QString& profileId, int delta);

    /**
     * @brief Returns the absolute path to a profile's config.json.
     * @param profileId Profile slug identifier.
     * @return Full path string.
     */
    static QString getProfileConfigPath(const QString& profileId);

    /**
     * @brief Checks whether a profile exists in the registry.
     * @param profileId Profile slug identifier.
     * @return True if present in profiles.json.
     */
    static bool profileExists(const QString& profileId);

    /**
     * @brief Migrates legacy single-profile config.json into default profile structure if needed.
     */
    static void migrateIfNeeded();

    /**
     * @brief Gets the absolute path to ~/.config/gutter-deck/profiles.json.
     * @return Registry file path string.
     */
    static QString getProfilesRegistryPath();

    /**
     * @brief Reads the legacy auto_launch profile slug if set.
     * @return Profile slug, or empty QString if unset.
     */
    static QString getAutoLaunchProfile();

    /**
     * @brief Sets or clears the legacy auto_launch profile setting in profiles.json.
     * @param profileId Profile slug to persist, or empty QString to clear.
     */
    static void setAutoLaunchProfile(const QString& profileId);

    /**
     * @brief Constructs a ConfigManager bound to a profile slug or direct file path.
     * @param profileIdOrCustomPath Profile ID or explicit .json config file path.
     */
    explicit ConfigManager(const QString& profileIdOrCustomPath = QString());
    ~ConfigManager() = default;

    /**
     * @brief Gets the loaded profile ID.
     * @return Profile slug identifier, or empty string for custom paths.
     */
    [[nodiscard]] QString getProfileId() const noexcept;

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
    QString m_profileId;
    QString m_customConfigPath;
    QVector<DeckConfig> m_decks;
    AppSettings m_settings;

    void populateDefaultConfiguration();
    void populateDefaultDecks();
};
