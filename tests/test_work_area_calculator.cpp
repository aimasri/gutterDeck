#include <QtTest/QtTest>
#include "infrastructure/WorkAreaCalculator.h"

/**
 * @brief Unit test suite for WorkAreaCalculator per-monitor strut resolution.
 * @details All fixtures are derived from the user's real environment, not invented values:
 *          - Laptop: `xrandr --listmonitors` → eDP-1 1920x1080+0+0 (root 1920x1080).
 *            Live tint2 strut: _NET_WM_STRUT_PARTIAL = 0,0,0,29, 0,0, 0,0, 0,0, 0,1919.
 *            Live Openbox: _NET_WORKAREA = 0,1,1920,1050 (rc.xml <margins><top>1</top>).
 *          - Dual head: DP-0 1920x1080+1080+840 (primary), HDMI-0 1080x1920+0+0 (portrait).
 *            Root = 3000x1920. tint2rc: panel_monitor = primary, bottom, 100% width, same
 *            strut_policy, so its strut is the laptop's 29px bottom strut spanning DP-0's
 *            x range [1080, 2999]. DP-0's bottom edge (840+1080) is flush with the root bottom.
 *          Edge-case tests reuse these monitor geometries with deliberately malformed struts.
 */
class TestWorkAreaCalculator : public QObject {
    Q_OBJECT

private:
    // Laptop (single head)
    const QRect kLaptopMonitor{0, 0, 1920, 1080};
    const QSize kLaptopRoot{1920, 1080};
    const QRect kLaptopNetWorkArea{0, 1, 1920, 1050};

    // Dual head
    const QRect kDp0{1080, 840, 1920, 1080};
    const QRect kHdmi0{0, 0, 1080, 1920};
    const QSize kDualRoot{3000, 1920};

    [[nodiscard]] static StrutReservation laptopTint2() {
        return StrutReservation::fromPartial({0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 1919});
    }
    [[nodiscard]] static StrutReservation dualHeadTint2() {
        return StrutReservation::fromPartial({0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 1080, 2999});
    }

private slots:
    // --- Real environment ---
    void testLaptopStrutOnly();
    void testLaptopWithNetWorkAreaHonoursOpenboxMargin();
    void testDualHeadPanelMonitorShrinks();
    void testDualHeadPortraitMonitorUnaffected();
    void testNoStrutsReturnsMonitor();

    // --- EWMH semantics ---
    void testLegacyStrutSpansWholeRootEdge();
    void testStackedPanelsSameEdgeTakeMaximum();
    void testAllFourEdges();

    // --- Malformed input / robustness ---
    void testZeroSpanPartialTreatedAsFullEdge();
    void testInvertedSpanIgnored();
    void testStrutConsumingWholeMonitorFallsBack();
    void testOversizedValuesClampedWithoutOverflow();
    void testDisjointNetWorkAreaIgnored();
    void testEmptyRootIgnoresStruts();
};

void TestWorkAreaCalculator::testLaptopStrutOnly() {
    const QRect r = WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {laptopTint2()});
    // Panel top edge = 1080 - 29 = 1051 → usable rows 0..1050.
    QCOMPARE(r, QRect(0, 0, 1920, 1051));
}

void TestWorkAreaCalculator::testLaptopWithNetWorkAreaHonoursOpenboxMargin() {
    const QRect r = WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {laptopTint2()},
                                                kLaptopNetWorkArea);
    // Single-head policy: intersect with _NET_WORKAREA → Openbox's 1px top margin is honoured.
    QCOMPARE(r, QRect(0, 1, 1920, 1050));
}

void TestWorkAreaCalculator::testDualHeadPanelMonitorShrinks() {
    const QRect r = WorkAreaCalculator::compute(kDp0, kDualRoot, {dualHeadTint2()});
    // Bottom = 1920 - 29 - 1 = 1890 → height = 1890 - 840 + 1 = 1051.
    QCOMPARE(r, QRect(1080, 840, 1920, 1051));
}

void TestWorkAreaCalculator::testDualHeadPortraitMonitorUnaffected() {
    const QRect r = WorkAreaCalculator::compute(kHdmi0, kDualRoot, {dualHeadTint2()});
    // Strut span x ∈ [1080, 2999] never touches HDMI-0 (x ∈ [0, 1079]).
    QCOMPARE(r, kHdmi0);
}

void TestWorkAreaCalculator::testNoStrutsReturnsMonitor() {
    QCOMPARE(WorkAreaCalculator::compute(kHdmi0, kDualRoot, {}), kHdmi0);
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {}), kLaptopMonitor);
}

void TestWorkAreaCalculator::testLegacyStrutSpansWholeRootEdge() {
    // EWMH: legacy _NET_WM_STRUT has no span → applies across the full root edge.
    const auto legacy = StrutReservation::fromLegacy({0, 0, 0, 29}, kDualRoot);
    QCOMPARE(legacy.bottomStartX, 0u);
    QCOMPARE(legacy.bottomEndX, 2999u);

    // Both monitors touch the root bottom edge, so both shrink by the same rows.
    QCOMPARE(WorkAreaCalculator::compute(kDp0, kDualRoot, {legacy}), QRect(1080, 840, 1920, 1051));
    QCOMPARE(WorkAreaCalculator::compute(kHdmi0, kDualRoot, {legacy}), QRect(0, 0, 1080, 1891));
}

void TestWorkAreaCalculator::testStackedPanelsSameEdgeTakeMaximum() {
    const auto second = StrutReservation::fromPartial({0, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 1919});
    const QRect r = WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {laptopTint2(), second});
    QCOMPARE(r, QRect(0, 0, 1920, 1080 - 59));
}

void TestWorkAreaCalculator::testAllFourEdges() {
    const auto left = StrutReservation::fromPartial({29, 0, 0, 0, 0, 1079, 0, 0, 0, 0, 0, 0});
    const auto right = StrutReservation::fromPartial({0, 29, 0, 0, 0, 0, 0, 1079, 0, 0, 0, 0});
    const auto top = StrutReservation::fromPartial({0, 0, 29, 0, 0, 0, 0, 0, 0, 1919, 0, 0});
    const QRect r = WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot,
                                                {left, right, top, laptopTint2()});
    QCOMPARE(r, QRect(QPoint(29, 29), QPoint(1920 - 29 - 1, 1080 - 29 - 1)));
}

void TestWorkAreaCalculator::testZeroSpanPartialTreatedAsFullEdge() {
    // Buggy clients publish all-zero spans; treated as a full-edge reservation.
    const auto zeroSpan = StrutReservation::fromPartial({0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 0, 0});
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {zeroSpan}),
             QRect(0, 0, 1920, 1051));
}

void TestWorkAreaCalculator::testInvertedSpanIgnored() {
    const auto inverted = StrutReservation::fromPartial({0, 0, 0, 29, 0, 0, 0, 0, 0, 0, 1919, 0});
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {inverted}), kLaptopMonitor);
}

void TestWorkAreaCalculator::testStrutConsumingWholeMonitorFallsBack() {
    const auto whole = StrutReservation::fromPartial({0, 0, 0, 1080, 0, 0, 0, 0, 0, 0, 0, 1919});
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {whole}), kLaptopMonitor);
}

void TestWorkAreaCalculator::testOversizedValuesClampedWithoutOverflow() {
    const auto bogus = StrutReservation::fromPartial(
        {0, 0, 0, 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0, 0, 0xFFFFFFFFu});
    // Clamped to the root height → consumes the monitor → degenerate fallback, no UB.
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, kLaptopRoot, {bogus}), kLaptopMonitor);
}

void TestWorkAreaCalculator::testDisjointNetWorkAreaIgnored() {
    const QRect disjoint(kHdmi0);  // HDMI-0 (x 0..1079) never overlaps DP-0 (x 1080..2999).
    const QRect r = WorkAreaCalculator::compute(kDp0, kDualRoot, {dualHeadTint2()}, disjoint);
    QCOMPARE(r, QRect(1080, 840, 1920, 1051));
}

void TestWorkAreaCalculator::testEmptyRootIgnoresStruts() {
    QCOMPARE(WorkAreaCalculator::compute(kLaptopMonitor, QSize(), {laptopTint2()}), kLaptopMonitor);
}

QTEST_APPLESS_MAIN(TestWorkAreaCalculator)
#include "test_work_area_calculator.moc"
