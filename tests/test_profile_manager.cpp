#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include "../src/infrastructure/ConfigManager.h"

class TestProfileManager : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        // Clean up any previous test runs
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck");
        if (dir.exists()) {
            dir.removeRecursively();
        }
    }

    void testMigrationFromLegacyConfig() {
        QString basePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/gutter-deck";
        QDir().mkpath(basePath);
        QFile legacyConfig(basePath + "/config.json");
        QVERIFY(legacyConfig.open(QIODevice::WriteOnly));
        legacyConfig.write("{\"settings\":{}, \"decks\":[]}");
        legacyConfig.close();

        ConfigManager::migrateIfNeeded();

        QVERIFY(QFile::exists(basePath + "/profiles.json"));
        QVERIFY(QFile::exists(basePath + "/config.json.backup"));
        QVERIFY(QFile::exists(basePath + "/profiles/default/config.json"));
        
        auto profiles = ConfigManager::listProfiles();
        QCOMPARE(profiles.size(), 1);
        QCOMPARE(profiles[0].id, QString("default"));
    }

    void testCreateProfile() {
        QVERIFY(ConfigManager::createProfile("Work Profile", QColor("#FF0000")));
        auto profiles = ConfigManager::listProfiles();
        QCOMPARE(profiles.size(), 2);
        QCOMPARE(profiles[1].id, QString("work-profile"));
        QCOMPARE(profiles[1].displayName, QString("Work Profile"));
        
        QString path = ConfigManager::getProfileConfigPath("work-profile");
        QVERIFY(QFile::exists(path));
    }

    void testDeleteProfile() {
        QVERIFY(ConfigManager::deleteProfile("work-profile"));
        auto profiles = ConfigManager::listProfiles();
        QCOMPARE(profiles.size(), 1);
        
        QString path = ConfigManager::getProfileConfigPath("work-profile");
        QVERIFY(!QFile::exists(path));
        
        // Refuse to delete the last one
        QVERIFY(!ConfigManager::deleteProfile("default"));
    }

    void testRenameProfile() {
        QVERIFY(ConfigManager::renameProfile("default", "Home Base"));
        auto profiles = ConfigManager::listProfiles();
        QCOMPARE(profiles[0].displayName, QString("Home Base"));
    }
    
    void testAutoLaunchPersistence() {
        ConfigManager::setAutoLaunchProfile("default");
        QCOMPARE(ConfigManager::getAutoLaunchProfile(), QString("default"));
        
        ConfigManager::setAutoLaunchProfile("");
        QCOMPARE(ConfigManager::getAutoLaunchProfile(), QString());
    }
};

QTEST_MAIN(TestProfileManager)
#include "test_profile_manager.moc"
