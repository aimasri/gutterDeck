#include <QtTest/QtTest>
#include <QTemporaryDir>
#include "infrastructure/ConfigManager.h"

/**
 * @brief Unit test suite for ConfigManager serialization, deserialization, and validation.
 */
class TestConfigManager : public QObject {
    Q_OBJECT

private slots:
    void testDefaultConfig();
    void testSerializationRoundTrip();
    void testValidationClamping();
    void testLegacyGuttersSupport();
    void testDeckCRUDOperations();
    void testInsertAndReorderDecks();
};

/**
 * @brief Number and identity of decks produced by ConfigManager::populateDefaultDecks().
 * @note Must mirror populateDefaultDecks() (commit 444f822: Chromium "gutter_1",
 *       PCManFM "gutter_2", Geany "gutter_3"). Centralised here so a future change to the
 *       defaults updates one place instead of silently breaking several assertions.
 */
namespace DefaultDecks {
constexpr int kCount = 3;
const QString kFirstId = QStringLiteral("gutter_1");
const QString kSecondId = QStringLiteral("gutter_2");
const QString kThirdId = QStringLiteral("gutter_3");
} // namespace DefaultDecks

void TestConfigManager::testDefaultConfig() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    ConfigManager manager(configPath);
    QVERIFY(manager.loadConfig());

    const auto& decks = manager.getDecks();
    QVERIFY(!decks.isEmpty());
    QCOMPARE(decks.size(), DefaultDecks::kCount);
    QCOMPARE(decks[0].id, DefaultDecks::kFirstId);
    QCOMPARE(decks[1].id, DefaultDecks::kSecondId);
    QCOMPARE(decks[2].id, DefaultDecks::kThirdId);
    for (const auto& deck : decks) {
        QVERIFY(!deck.id.isEmpty());
        QVERIFY(!deck.command.isEmpty());
        QVERIFY(deck.color.isValid());
    }

    const auto& settings = manager.getSettings();
    QCOMPARE(settings.gutterWidth, ConfigDefaults::GUTTER_WIDTH);
    QCOMPARE(settings.animationDurationMs, ConfigDefaults::ANIMATION_DURATION_MS);
    QCOMPARE(settings.targetWorkspace, -1);
}

void TestConfigManager::testSerializationRoundTrip() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    // 1. Write custom settings
    {
        ConfigManager writer(configPath);
        AppSettings s;
        s.gutterWidth = 25;
        s.gutterExpandedWidth = 60;
        s.animationDurationMs = 400;
        s.swellDurationMs = 150;
        s.targetWorkspace = 2;
        writer.setSettings(s);

        QVector<DeckConfig> decks;
        DeckConfig d;
        d.id = "custom_1";
        d.name = "My Custom App";
        d.command = "firefox";
        d.color = QColor("#FF112233");
        decks.append(d);
        writer.setDecks(decks);

        QVERIFY(writer.saveConfig());
    }

    // 2. Read back with new instance
    {
        ConfigManager reader(configPath);
        QVERIFY(reader.loadConfig());

        const auto& s = reader.getSettings();
        QCOMPARE(s.gutterWidth, 25);
        QCOMPARE(s.gutterExpandedWidth, 60);
        QCOMPARE(s.animationDurationMs, 400);
        QCOMPARE(s.swellDurationMs, 150);
        QCOMPARE(s.targetWorkspace, 2);

        const auto& decks = reader.getDecks();
        QCOMPARE(decks.size(), 1);
        QCOMPARE(decks[0].id, QString("custom_1"));
        QCOMPARE(decks[0].name, QString("My Custom App"));
        QCOMPARE(decks[0].command, QString("firefox"));
        QCOMPARE(decks[0].color.name(QColor::HexArgb), QString("#ff112233"));
    }
}

void TestConfigManager::testValidationClamping() {
    ConfigManager manager;
    AppSettings s;
    s.gutterWidth = -10;             // Must clamp to >= 2
    s.gutterExpandedWidth = 2;       // Must clamp to >= gutterWidth + 4
    s.animationDurationMs = 99999;   // Must clamp to <= 2000
    s.swellDurationMs = 1;           // Must clamp to >= 50
    manager.setSettings(s);

    const auto& validated = manager.getSettings();
    QCOMPARE(validated.gutterWidth, 2);
    QCOMPARE(validated.gutterExpandedWidth, 6);
    QCOMPARE(validated.animationDurationMs, 2000);
    QCOMPARE(validated.swellDurationMs, 50);
}

void TestConfigManager::testLegacyGuttersSupport() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    // Write a legacy-format config file with "gutters" instead of "decks"
    {
        QFile file(configPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(R"({
            "gutters": [
                {
                    "id": "gutter_legacy",
                    "name": "Legacy Deck",
                    "command": "legacy-cmd",
                    "color": "#80112233"
                }
            ],
            "settings": {
                "screen_width": 1080,
                "screen_height": 1920,
                "target_screen": "HDMI-1"
            }
        })");
        file.close();
    }

    ConfigManager reader(configPath);
    QVERIFY(reader.loadConfig());

    const auto& decks = reader.getDecks();
    QCOMPARE(decks.size(), 1);
    QCOMPARE(decks[0].id, QString("gutter_legacy"));
    QCOMPARE(decks[0].name, QString("Legacy Deck"));
    QCOMPARE(decks[0].command, QString("legacy-cmd"));

    const auto& settings = reader.getSettings();
    QCOMPARE(settings.screenWidth, 1080);
    QCOMPARE(settings.screenHeight, 1920);
}

void TestConfigManager::testDeckCRUDOperations() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    ConfigManager manager(configPath);
    QVERIFY(manager.loadConfig());
    QCOMPARE(manager.getDecks().size(), DefaultDecks::kCount);

    // 1. Update deck
    QVERIFY(manager.updateDeck(0, "Updated Browser", "firefox --new-window", QColor("#80112233")));
    QCOMPARE(manager.getDecks()[0].name, QString("Updated Browser"));
    QCOMPARE(manager.getDecks()[0].command, QString("firefox --new-window"));
    QCOMPARE(manager.getDecks()[0].color, QColor("#80112233"));

    // 2. Add deck (appended after the defaults)
    DeckConfig newDeck;
    newDeck.id = "custom_4";
    newDeck.name = "Code Editor";
    newDeck.command = "code";
    newDeck.color = QColor("#80334455");
    QVERIFY(manager.addDeck(newDeck));
    QCOMPARE(manager.getDecks().size(), DefaultDecks::kCount + 1);
    QCOMPARE(manager.getDecks()[DefaultDecks::kCount].id, QString("custom_4"));

    // 3. Remove deck at index 1 → remaining: [gutter_1, gutter_3, custom_4]
    QVERIFY(manager.removeDeck(1));
    QCOMPARE(manager.getDecks().size(), DefaultDecks::kCount);
    QCOMPARE(manager.getDecks()[1].id, DefaultDecks::kThirdId);
    QCOMPARE(manager.getDecks()[2].id, QString("custom_4"));

    // 4. Boundary check: remove until 1 deck
    while (manager.getDecks().size() > 1) {
        QVERIFY(manager.removeDeck(manager.getDecks().size() - 1));
    }
    QCOMPARE(manager.getDecks().size(), 1);

    // Refuse to delete last deck
    QVERIFY(!manager.removeDeck(0));
    QCOMPARE(manager.getDecks().size(), 1);

    // 5. Verify persistence across new instance
    ConfigManager reader(configPath);
    QVERIFY(reader.loadConfig());
    QCOMPARE(reader.getDecks().size(), 1);
    QCOMPARE(reader.getDecks()[0].name, QString("Updated Browser"));
}

void TestConfigManager::testInsertAndReorderDecks() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    ConfigManager manager(configPath);
    QVERIFY(manager.loadConfig());
    QCOMPARE(manager.getDecks().size(), DefaultDecks::kCount);

    // Insert at index 0 (Left of first deck) → [deck_left, gutter_1, gutter_2, gutter_3]
    DeckConfig deckLeft;
    deckLeft.id = "deck_left";
    deckLeft.name = "Leftmost Deck";
    deckLeft.command = "xterm";
    deckLeft.color = QColor("#80112233");
    QVERIFY(manager.insertDeck(0, deckLeft));
    QCOMPARE(manager.getDecks().size(), 4);
    QCOMPARE(manager.getDecks()[0].id, QString("deck_left"));

    // Insert at index 2 → [deck_left, gutter_1, deck_mid, gutter_2, gutter_3]
    DeckConfig deckMid;
    deckMid.id = "deck_mid";
    deckMid.name = "Middle Deck";
    deckMid.command = "xterm";
    deckMid.color = QColor("#80445566");
    QVERIFY(manager.insertDeck(2, deckMid));
    QCOMPARE(manager.getDecks().size(), 5);
    QCOMPARE(manager.getDecks()[2].id, QString("deck_mid"));

    // Reorder decks: full reversal [4, 3, 2, 1, 0]
    // → [gutter_3, gutter_2, deck_mid, gutter_1, deck_left]
    QVector<int> revOrder = {4, 3, 2, 1, 0};
    manager.reorderDecks(revOrder);
    QCOMPARE(manager.getDecks().size(), 5);
    QCOMPARE(manager.getDecks()[0].id, DefaultDecks::kThirdId);
    QCOMPARE(manager.getDecks()[1].id, DefaultDecks::kSecondId);
    QCOMPARE(manager.getDecks()[2].id, QString("deck_mid"));
    QCOMPARE(manager.getDecks()[3].id, DefaultDecks::kFirstId);
    QCOMPARE(manager.getDecks()[4].id, QString("deck_left"));

    // Verify persistence across new instance
    ConfigManager reader(configPath);
    QVERIFY(reader.loadConfig());
    QCOMPARE(reader.getDecks().size(), 5);
    QCOMPARE(reader.getDecks()[0].id, DefaultDecks::kThirdId);
    QCOMPARE(reader.getDecks()[1].id, DefaultDecks::kSecondId);
    QCOMPARE(reader.getDecks()[2].id, QString("deck_mid"));
    QCOMPARE(reader.getDecks()[3].id, DefaultDecks::kFirstId);
    QCOMPARE(reader.getDecks()[4].id, QString("deck_left"));
}

QTEST_MAIN(TestConfigManager)
#include "test_config_manager.moc"
