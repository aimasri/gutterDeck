#include <QTest>
#include <QLineEdit>
#include <QSlider>
#include <QRadioButton>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include "presentation/SleekDialogs.h"
#include "presentation/GutterWidget.h"

class TestSleekDialogs : public QObject {
    Q_OBJECT

private slots:
    void testContextMenuProperties();
    void testInputDialog();
    void testColorDialogProperties();
    void testAddDeckDialogProperties();
    void testReorderDialogProperties();
    void testDarkModePaletteAndSwatches();
    void testGutterWidgetSwellStages();
    void testGutterWidgetInactiveZone();
    void testGutterWidgetWheelNavigation();
};

void TestSleekDialogs::testContextMenuProperties() {
    SleekContextMenu menu;
    QVERIFY(menu.windowFlags() & Qt::Popup);
    QVERIFY(menu.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(menu.testAttribute(Qt::WA_TranslucentBackground));

    const SleekMenuIcon allIcons[] = {
        SleekMenuIcon::EditName,
        SleekMenuIcon::EditCommand,
        SleekMenuIcon::ChangeColor,
        SleekMenuIcon::AddDeck,
        SleekMenuIcon::ReorderDecks,
        SleekMenuIcon::DeleteDeck,
        SleekMenuIcon::CloseApp
    };

    for (auto iconType : allIcons) {
        QIcon icon = getSleekMenuIcon(iconType);
        QVERIFY(!icon.isNull());
        QPixmap pm = icon.pixmap(32, 32);
        QVERIFY(!pm.isNull());
        QCOMPARE(pm.width(), 32);
        QCOMPARE(pm.height(), 32);
    }

    auto* act1 = menu.addAction(getSleekMenuIcon(SleekMenuIcon::EditName), "Item 1");
    auto* act2 = menu.addAction(getSleekMenuIcon(SleekMenuIcon::DeleteDeck), "Item 2");
    QVERIFY(act1 != nullptr);
    QVERIFY(act2 != nullptr);
    QVERIFY(!act1->icon().isNull());
}

void TestSleekDialogs::testInputDialog() {
    SleekInputDialog dlg("Test Title", "Test Subtitle", "Default Value");
    QVERIFY(dlg.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(dlg.testAttribute(Qt::WA_TranslucentBackground));
    QCOMPARE(dlg.value(), QString("Default Value"));

    auto* lineEdit = dlg.findChild<QLineEdit*>();
    QVERIFY(lineEdit != nullptr);
    lineEdit->setText("Modified Value");
    QCOMPARE(dlg.value(), QString("Modified Value"));
}

void TestSleekDialogs::testColorDialogProperties() {
    QColor startColor("#6366f1");
    startColor.setAlphaF(0.8);

    SleekColorDialog dlg(startColor);
    QVERIFY(dlg.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(dlg.testAttribute(Qt::WA_TranslucentBackground));

    QCOMPARE(dlg.selectedColor().name(QColor::HexRgb), startColor.name(QColor::HexRgb));
    QCOMPARE(static_cast<int>(dlg.selectedColor().alphaF() * 100.0 + 0.5), 80);

    // Test slider modification
    auto* slider = dlg.findChild<QSlider*>();
    QVERIFY(slider != nullptr);
    slider->setValue(40);
    QCOMPARE(static_cast<int>(dlg.selectedColor().alphaF() * 100.0 + 0.5), 40);

    // Test hex edit modification
    auto* hexEdit = dlg.findChild<QLineEdit*>();
    QVERIFY(hexEdit != nullptr);
    hexEdit->setText("#10b981");
    QCOMPARE(dlg.selectedColor().name(QColor::HexRgb).toLower(), QString("#10b981"));
}

void TestSleekDialogs::testAddDeckDialogProperties() {
    SleekAddDeckDialog dlg("Main Deck");
    QVERIFY(dlg.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(dlg.testAttribute(Qt::WA_TranslucentBackground));

    // Fallbacks to placeholder defaults when empty
    QCOMPARE(dlg.deckName(), QString("New Deck"));
    QCOMPARE(dlg.deckCommand(), QString("x-terminal-emulator"));
    QVERIFY(dlg.deckColor().isValid());

    auto lineEdits = dlg.findChildren<QLineEdit*>();
    QVERIFY(lineEdits.size() >= 2);
    lineEdits[0]->setText("Browser");
    lineEdits[1]->setText("google-chrome");

    QCOMPARE(dlg.deckName(), QString("Browser"));
    QCOMPARE(dlg.deckCommand(), QString("google-chrome"));

    // Placement radio buttons
    auto radios = dlg.findChildren<QRadioButton*>();
    QCOMPARE(radios.size(), 2);
    // Right is default
    QVERIFY(!dlg.insertLeft());
    // Toggle Left
    radios[0]->setChecked(true);
    QVERIFY(dlg.insertLeft());
}

void TestSleekDialogs::testReorderDialogProperties() {
    QVector<DeckItemInfo> items = {
        {0, "Browser", "firefox", QColor("#6366f1")},
        {1, "Editor", "nvim", QColor("#10b981")},
        {2, "Terminal", "alacritty", QColor("#ec4899")}
    };

    SleekReorderDialog dlg(items, 1);
    QVERIFY(dlg.windowFlags() & Qt::FramelessWindowHint);
    QVERIFY(dlg.testAttribute(Qt::WA_TranslucentBackground));

    auto* listWidget = dlg.findChild<QListWidget*>();
    QVERIFY(listWidget != nullptr);
    QCOMPARE(listWidget->count(), 3);

    // Initial order should be [0, 1, 2]
    QCOMPARE(dlg.newOrder(), QVector<int>({0, 1, 2}));

    // Initially active item 1 is selected
    QCOMPARE(listWidget->currentRow(), 1);

    // Find Up and Down buttons
    auto buttons = dlg.findChildren<QPushButton*>();
    QPushButton* upBtn = nullptr;
    QPushButton* downBtn = nullptr;
    for (auto* btn : buttons) {
        if (btn->text().contains("Up")) upBtn = btn;
        if (btn->text().contains("Down")) downBtn = btn;
    }
    QVERIFY(upBtn != nullptr);
    QVERIFY(downBtn != nullptr);

    // Move item 1 ("Editor") UP -> order becomes [1, 0, 2]
    upBtn->click();
    QCOMPARE(listWidget->currentRow(), 0);
    QCOMPARE(dlg.newOrder(), QVector<int>({1, 0, 2}));

    // At top row (0), upBtn should be disabled
    QVERIFY(!upBtn->isEnabled());

    // Move item 1 DOWN twice -> order becomes [0, 2, 1]
    downBtn->click(); // [0, 1, 2]
    downBtn->click(); // [0, 2, 1]
    QCOMPARE(listWidget->currentRow(), 2);
    QCOMPARE(dlg.newOrder(), QVector<int>({0, 2, 1}));

    // At bottom row (2), downBtn should be disabled
    QVERIFY(!downBtn->isEnabled());
}

void TestSleekDialogs::testDarkModePaletteAndSwatches() {
    const auto& palette = getSleekDarkPalette();
    QCOMPARE(palette.size(), 24);

    // Verify all colors are valid hex codes
    for (const auto& hex : palette) {
        QColor c(hex);
        QVERIFY2(c.isValid(), qPrintable(QString("Invalid color: %1").arg(hex)));
    }

    // Verify presence of key dark mode hue families
    // Blues
    QVERIFY(palette.contains(QStringLiteral("#172554")));
    QVERIFY(palette.contains(QStringLiteral("#2563eb")));
    QVERIFY(palette.contains(QStringLiteral("#38bdf8"))); // bright accent

    // Greens
    QVERIFY(palette.contains(QStringLiteral("#14532d")));
    QVERIFY(palette.contains(QStringLiteral("#16a34a")));
    QVERIFY(palette.contains(QStringLiteral("#10b981"))); // bright accent

    // Reds
    QVERIFY(palette.contains(QStringLiteral("#7f1d1d")));
    QVERIFY(palette.contains(QStringLiteral("#dc2626")));
    QVERIFY(palette.contains(QStringLiteral("#f43f5e"))); // bright accent

    // Oranges
    QVERIFY(palette.contains(QStringLiteral("#7c2d12")));
    QVERIFY(palette.contains(QStringLiteral("#c2410c")));
    QVERIFY(palette.contains(QStringLiteral("#fb923c"))); // bright accent

    // Verify SleekColorDialog swatches
    SleekColorDialog colorDlg(QColor("#172554"));
    auto colorButtons = colorDlg.findChildren<QPushButton*>();
    // SleekColorDialog has 24 swatch buttons + Cancel + Apply buttons = 26 buttons
    int swatchCount = 0;
    for (auto* btn : colorButtons) {
        if (btn->maximumSize() == QSize(28, 28)) {
            swatchCount++;
        }
    }
    QCOMPARE(swatchCount, 24);

    // Click on a bright accent swatch (e.g. #38bdf8)
    for (auto* btn : colorButtons) {
        if (btn->styleSheet().contains("#38bdf8")) {
            btn->click();
            break;
        }
    }
    QCOMPARE(colorDlg.selectedColor().name(QColor::HexRgb).toLower(), QString("#38bdf8"));

    // Verify SleekAddDeckDialog swatches
    SleekAddDeckDialog addDlg("Deck 1");
    auto addButtons = addDlg.findChildren<QPushButton*>();
    int addSwatchCount = 0;
    for (auto* btn : addButtons) {
        if (btn->maximumSize() == QSize(26, 26)) {
            addSwatchCount++;
        }
    }
    QCOMPARE(addSwatchCount, 24);

    // Click on a bright orange accent swatch (e.g. #fb923c)
    for (auto* btn : addButtons) {
        if (btn->styleSheet().contains("#fb923c")) {
            btn->click();
            break;
        }
    }
    QCOMPARE(addDlg.deckColor().name(QColor::HexRgb).toLower(), QString("#fb923c"));
}

void TestSleekDialogs::testGutterWidgetSwellStages() {
    GutterWidget widget(0, "Test Deck", QColor("#806366f1"), 4, 44, 80);
    QCOMPARE(widget.swellStage(), SwellStage::Collapsed);
    QCOMPARE(widget.gutterWidth(), 4);

    // Transition to Stage 2: Halfway
    widget.setSwellStage(SwellStage::Halfway);
    QCOMPARE(widget.swellStage(), SwellStage::Halfway);
    QTest::qWait(120);
    QCOMPARE(widget.gutterWidth(), 24); // (4 + 44) / 2 = 24

    // Transition to Stage 3: Expanded
    widget.setSwellStage(SwellStage::Expanded);
    QCOMPARE(widget.swellStage(), SwellStage::Expanded);
    QTest::qWait(120);
    QCOMPARE(widget.gutterWidth(), 44);

    // Transition back to Stage 1: Collapsed
    widget.setSwellStage(SwellStage::Collapsed);
    QCOMPARE(widget.swellStage(), SwellStage::Collapsed);
    QTest::qWait(120);
    QCOMPARE(widget.gutterWidth(), 4);
}

void TestSleekDialogs::testGutterWidgetInactiveZone() {
    GutterWidget widget(0, "Test Deck", QColor("#806366f1"), 4, 44, 80);
    widget.resize(44, 800);

    QSignalSpy clickSpy(&widget, &GutterWidget::clicked);
    QSignalSpy enterSpy(&widget, &GutterWidget::hoverEntered);
    QSignalSpy leaveSpy(&widget, &GutterWidget::hoverLeft);

    // 1. Mouse click at y <= 80 (e.g. 50) should be ignored (not clickable)
    QMouseEvent pressTop(QEvent::MouseButtonPress, QPointF(10, 50), QPointF(10, 50), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &pressTop);
    QCOMPARE(clickSpy.count(), 0);

    // 2. Mouse click at y > 80 (e.g. 120) should be accepted
    QMouseEvent pressActive(QEvent::MouseButtonPress, QPointF(10, 120), QPointF(10, 120), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &pressActive);
    QCOMPARE(clickSpy.count(), 1);

    // 3. Enter event at y <= 80 (e.g. 50) should NOT swell
    QEnterEvent enterTop(QPointF(10, 50), QPointF(10, 50), QPointF(10, 50));
    QCoreApplication::sendEvent(&widget, &enterTop);
    QCOMPARE(enterSpy.count(), 0);

    // 4. Mouse move into y > 80 (e.g. 120) should swell
    QMouseEvent moveActive(QEvent::MouseMove, QPointF(10, 120), QPointF(10, 120), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &moveActive);
    QCOMPARE(enterSpy.count(), 1);

    // Set stage to Expanded to simulate swell
    widget.setSwellStage(SwellStage::Expanded);

    // 5. Mouse move into y <= 80 (e.g. 50) should trigger hoverLeft to collapse back
    QMouseEvent moveInactive(QEvent::MouseMove, QPointF(10, 50), QPointF(10, 50), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&widget, &moveInactive);
    QCOMPARE(leaveSpy.count(), 1);
}

void TestSleekDialogs::testGutterWidgetWheelNavigation() {
    GutterWidget widget(0, "Test Deck", QColor("#806366f1"), 4, 44, 80);
    widget.resize(44, 800);

    QSignalSpy prevSpy(&widget, &GutterWidget::previousRequested);
    QSignalSpy nextSpy(&widget, &GutterWidget::nextRequested);

    // 1. Wheel scroll in inactive top zone (y <= 80) should be ignored
    QWheelEvent wheelTop(QPointF(10, 50), QPointF(10, 50), QPoint(0, 0), QPoint(0, 120),
                         Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&widget, &wheelTop);
    QCOMPARE(prevSpy.count(), 0);
    QCOMPARE(nextSpy.count(), 0);

    // 2. Wheel scroll up in active zone (delta Y = +120) -> previous deck
    QWheelEvent wheelUp(QPointF(10, 120), QPointF(10, 120), QPoint(0, 0), QPoint(0, 120),
                        Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&widget, &wheelUp);
    QCOMPARE(prevSpy.count(), 1);
    QCOMPARE(nextSpy.count(), 0);

    // 3. Wheel scroll down in active zone (delta Y = -120) -> next deck
    QWheelEvent wheelDown(QPointF(10, 120), QPointF(10, 120), QPoint(0, 0), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&widget, &wheelDown);
    QCOMPARE(prevSpy.count(), 1);
    QCOMPARE(nextSpy.count(), 1);
}

QTEST_MAIN(TestSleekDialogs)
#include "test_sleek_dialogs.moc"
