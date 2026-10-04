#pragma once

#include <QRect>
#include <QSize>
#include <QVector>
#include <array>
#include <cstdint>
#include <optional>

/**
 * @brief Value object describing one client's EWMH screen-edge reservation (strut).
 * @details Mirrors the 12-cardinal layout of _NET_WM_STRUT_PARTIAL:
 *          left, right, top, bottom, left_start_y, left_end_y, right_start_y, right_end_y,
 *          top_start_x, top_end_x, bottom_start_x, bottom_end_x.
 *          All thicknesses are measured from the edge of the ROOT window (not the monitor),
 *          and all span coordinates are inclusive root coordinates, exactly as defined by EWMH.
 * @note Plain aggregate with no X11 dependency so it can be constructed in unit tests.
 */
struct StrutReservation {
    uint32_t left = 0;
    uint32_t right = 0;
    uint32_t top = 0;
    uint32_t bottom = 0;
    uint32_t leftStartY = 0;
    uint32_t leftEndY = 0;
    uint32_t rightStartY = 0;
    uint32_t rightEndY = 0;
    uint32_t topStartX = 0;
    uint32_t topEndX = 0;
    uint32_t bottomStartX = 0;
    uint32_t bottomEndX = 0;

    /**
     * @brief Builds a reservation from a raw _NET_WM_STRUT_PARTIAL property payload.
     * @param v The 12 CARDINAL values in EWMH order.
     * @return Populated reservation.
     */
    [[nodiscard]] static StrutReservation fromPartial(const std::array<uint32_t, 12>& v) noexcept;

    /**
     * @brief Builds a reservation from a legacy 4-value _NET_WM_STRUT property payload.
     * @details EWMH defines the legacy strut as spanning the entire root edge, so the
     *          spans are expanded to [0, rootWidth-1] / [0, rootHeight-1].
     * @param v The 4 CARDINAL values (left, right, top, bottom).
     * @param root Current root window dimensions.
     * @return Populated reservation with full-edge spans.
     */
    [[nodiscard]] static StrutReservation fromLegacy(const std::array<uint32_t, 4>& v,
                                                     const QSize& root) noexcept;

    /**
     * @brief Checks whether the reservation reserves any space at all.
     * @return True if all four thicknesses are zero.
     */
    [[nodiscard]] bool isEmpty() const noexcept;
};

/**
 * @brief Stateless resolver that computes the usable work area of a single monitor.
 * @details Window managers compute per-monitor work areas internally, but EWMH only exports
 *          a single per-desktop rectangle (_NET_WORKAREA), which is ambiguous on multi-head
 *          systems (Qt's own xcb backend documents this). This class re-implements the WM's
 *          per-monitor calculation from the raw struts:
 *          1. Each strut edge is converted into a rectangle in root coordinates.
 *          2. Only strut rectangles that INTERSECT the target monitor shrink it, so a panel on
 *             a neighbouring monitor never steals space from this one.
 *          3. Multiple struts on the same edge accumulate as a maximum (stacked panels).
 *          4. Optionally, the result is intersected with _NET_WORKAREA (callers pass it only
 *             on single-head setups, where it is authoritative and also captures WM-internal
 *             reservations such as Openbox <margins>).
 * @note Pure function, no Qt event loop, no X11. Thread-safe and fully unit-testable.
 *       Teaching note: think of this as a PHP "Domain Service" with only static methods —
 *       all inputs arrive as arguments, nothing is cached.
 */
class WorkAreaCalculator {
public:
    WorkAreaCalculator() = delete;

    /**
     * @brief Computes the usable area of @p monitor after subtracting panel struts.
     * @details Edge cases handled:
     *          - Strut thicknesses/spans larger than the root are clamped (malformed clients).
     *          - Partial spans with start == end == 0 are treated as full-edge spans; several
     *            panels publish this pattern instead of real coordinates.
     *          - Partial spans with end < start are malformed and ignored.
     *          - A @p netWorkArea that does not overlap the computed area is ignored.
     *          - A degenerate (empty) result falls back to the full monitor so that a bogus
     *            reservation can never collapse the deck to zero size.
     * @param monitor Monitor geometry in root coordinates.
     * @param root Root window dimensions (needed because struts are root-edge relative).
     * @param struts All reservations relevant to the target desktop.
     * @param netWorkArea Optional _NET_WORKAREA rectangle to intersect with (single-head only).
     * @return Usable work area in root coordinates.
     */
    [[nodiscard]] static QRect compute(const QRect& monitor,
                                       const QSize& root,
                                       const QVector<StrutReservation>& struts,
                                       const std::optional<QRect>& netWorkArea = std::nullopt);
};
