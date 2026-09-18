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

void TestConfigManager::testDefaultConfig() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString configPath = tempDir.path() + "/config.json";

    ConfigManager manager(configPath);
    QVERIFY(manager.loadConfig());

    const auto& decks = manager.getDecks();
    QVERIFY(!decks.isEmpty());
    QCOMPARE(decks.size(), 2);
    QVERIFY(!decks[0].id.isEmpty());
    QVERIFY(!decks[0].command.isEmpty());
    QVERIFY(decks[0].color.isValid());

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
    QCOMPARE(manager.getDecks().size(), 2);

    // 1. Update deck
    QVERIFY(manager.updateDeck(0, "Updated Browser", "firefox --new-window", QColor("#80112233")));
    QCOMPARE(manager.getDecks()[0].name, QString("Updated Browser"));
    QCOMPARE(manager.getDecks()[0].command, QString("firefox --new-window"));
    QCOMPARE(manager.getDecks()[0].color, QColor("#80112233"));

    // 2. Add deck
    DeckConfig newDeck;
    newDeck.id = "custom_3";
    newDeck.name = "Editor";
    newDeck.command = "code";
    newDeck.color = QColor("#80334455");
    QVERIFY(manager.addDeck(newDeck));
    QCOMPARE(manager.getDecks().size(), 3);
    QCOMPARE(manager.getDecks()[2].name, QString("Editor"));

    // 3. Remove deck
    QVERIFY(manager.removeDeck(1));
    QCOMPARE(manager.getDecks().size(), 2);
    QCOMPARE(manager.getDecks()[1].name, QString("Editor"));

    // 4. Boundary check: remove until 1 deck
    QVERIFY(manager.removeDeck(1));
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
    QCOMPARE(manager.getDecks().size(), 2);

    // Insert at index 0 (Left of first deck)
    DeckConfig deckLeft;
    deckLeft.id = "deck_left";
    deckLeft.name = "Leftmost Deck";
    deckLeft.command = "xterm";
    deckLeft.color = QColor("#80112233");
    QVERIFY(manager.insertDeck(0, deckLeft));
    QCOMPARE(manager.getDecks().size(), 3);
    QCOMPARE(manager.getDecks()[0].id, QString("deck_left"));

    // Insert at index 2 (between 1 and 2)
    DeckConfig deckMid;
    deckMid.id = "deck_mid";
    deckMid.name = "Middle Deck";
    deckMid.command = "xterm";
    deckMid.color = QColor("#80445566");
    QVERIFY(manager.insertDeck(2, deckMid));
    QCOMPARE(manager.getDecks().size(), 4);
    QCOMPARE(manager.getDecks()[2].id, QString("deck_mid"));

    // Reorder decks: reverse order [3, 2, 1, 0]
    QVector<int> revOrder = {3, 2, 1, 0};
    manager.reorderDecks(revOrder);
    QCOMPARE(manager.getDecks().size(), 4);
    QCOMPARE(manager.getDecks()[0].id, QString("gutter_2"));
    QCOMPARE(manager.getDecks()[1].id, QString("deck_mid"));
    QCOMPARE(manager.getDecks()[3].id, QString("deck_left"));

    // Verify persistence across new instance
    ConfigManager reader(configPath);
    QVERIFY(reader.loadConfig());
    QCOMPARE(reader.getDecks().size(), 4);
    QCOMPARE(reader.getDecks()[0].id, QString("gutter_2"));
    QCOMPARE(reader.getDecks()[1].id, QString("deck_mid"));
    QCOMPARE(reader.getDecks()[3].id, QString("deck_left"));
}

QTEST_MAIN(TestConfigManager)
#include "test_config_manager.moc"
