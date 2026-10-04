#include "WorkAreaCalculator.h"

#include <algorithm>
#include <utility>

namespace {

/**
 * @brief Clamps a raw 32-bit strut value into [0, limit] using 64-bit arithmetic.
 * @details Prevents signed overflow when malformed clients publish values such as 0xFFFFFFFF.
 */
[[nodiscard]] int64_t clampToLimit(uint32_t value, int64_t limit) noexcept {
    return std::min<int64_t>(static_cast<int64_t>(value), std::max<int64_t>(limit, 0));
}

/**
 * @brief Resolves an inclusive EWMH span into a [start, end] pair clamped to the root extent.
 * @param start Raw span start.
 * @param end Raw span end (inclusive).
 * @param extent Root dimension along the span axis.
 * @return Resolved [start, end], or std::nullopt if the span is malformed (end < start).
 * @note start == end == 0 is treated as "unspecified" and expanded to the full edge;
 *       several panels publish all-zero spans instead of real coordinates.
 */
[[nodiscard]] std::optional<std::pair<int64_t, int64_t>> resolveSpan(uint32_t start, uint32_t end,
                                                                     int64_t extent) noexcept {
    if (extent <= 0) {
        return std::nullopt;
    }
    if (start == 0 && end == 0) {
        return std::make_pair(int64_t{0}, extent - 1);
    }
    if (end < start) {
        return std::nullopt;
    }
    const int64_t s = clampToLimit(start, extent - 1);
    const int64_t e = clampToLimit(end, extent - 1);
    return std::make_pair(s, e);
}

/**
 * @brief Inclusive 1-D overlap test between [a0, a1] and [b0, b1].
 */
[[nodiscard]] constexpr bool overlaps(int64_t a0, int64_t a1, int64_t b0, int64_t b1) noexcept {
    return a0 <= b1 && b0 <= a1;
}

} // namespace

StrutReservation StrutReservation::fromPartial(const std::array<uint32_t, 12>& v) noexcept {
    StrutReservation s;
    s.left = v[0];
    s.right = v[1];
    s.top = v[2];
    s.bottom = v[3];
    s.leftStartY = v[4];
    s.leftEndY = v[5];
    s.rightStartY = v[6];
    s.rightEndY = v[7];
    s.topStartX = v[8];
    s.topEndX = v[9];
    s.bottomStartX = v[10];
    s.bottomEndX = v[11];
    return s;
}

StrutReservation StrutReservation::fromLegacy(const std::array<uint32_t, 4>& v,
                                              const QSize& root) noexcept {
    const uint32_t maxX = root.width() > 0 ? static_cast<uint32_t>(root.width() - 1) : 0;
    const uint32_t maxY = root.height() > 0 ? static_cast<uint32_t>(root.height() - 1) : 0;

    StrutReservation s;
    s.left = v[0];
    s.right = v[1];
    s.top = v[2];
    s.bottom = v[3];
    s.leftStartY = 0;
    s.leftEndY = maxY;
    s.rightStartY = 0;
    s.rightEndY = maxY;
    s.topStartX = 0;
    s.topEndX = maxX;
    s.bottomStartX = 0;
    s.bottomEndX = maxX;
    return s;
}

bool StrutReservation::isEmpty() const noexcept {
    return left == 0 && right == 0 && top == 0 && bottom == 0;
}

QRect WorkAreaCalculator::compute(const QRect& monitor,
                                  const QSize& root,
                                  const QVector<StrutReservation>& struts,
                                  const std::optional<QRect>& netWorkArea) {
    if (!monitor.isValid() || monitor.isEmpty()) {
        return monitor;
    }

    const int64_t rootW = root.width();
    const int64_t rootH = root.height();

    // Monitor edges as inclusive root coordinates (QRect::right()/bottom() are inclusive).
    const int64_t monL = monitor.left();
    const int64_t monT = monitor.top();
    const int64_t monR = monitor.right();
    const int64_t monB = monitor.bottom();

    // Working edges, shrunk as intersecting struts are applied.
    int64_t workL = monL;
    int64_t workT = monT;
    int64_t workR = monR;
    int64_t workB = monB;

    if (rootW > 0 && rootH > 0) {
        for (const StrutReservation& s : struts) {
            if (s.isEmpty()) {
                continue;
            }

            // LEFT edge: strip occupies x ∈ [0, left-1], y ∈ [leftStartY, leftEndY].
            if (s.left > 0) {
                const int64_t thickness = clampToLimit(s.left, rootW);
                const auto span = resolveSpan(s.leftStartY, s.leftEndY, rootH);
                if (span && overlaps(0, thickness - 1, monL, monR) &&
                    overlaps(span->first, span->second, monT, monB)) {
                    workL = std::max(workL, thickness);
                }
            }

            // RIGHT edge: strip occupies x ∈ [rootW-right, rootW-1].
            if (s.right > 0) {
                const int64_t thickness = clampToLimit(s.right, rootW);
                const auto span = resolveSpan(s.rightStartY, s.rightEndY, rootH);
                if (span && overlaps(rootW - thickness, rootW - 1, monL, monR) &&
                    overlaps(span->first, span->second, monT, monB)) {
                    workR = std::min(workR, rootW - thickness - 1);
                }
            }

            // TOP edge: strip occupies y ∈ [0, top-1], x ∈ [topStartX, topEndX].
            if (s.top > 0) {
                const int64_t thickness = clampToLimit(s.top, rootH);
                const auto span = resolveSpan(s.topStartX, s.topEndX, rootW);
                if (span && overlaps(0, thickness - 1, monT, monB) &&
                    overlaps(span->first, span->second, monL, monR)) {
                    workT = std::max(workT, thickness);
                }
            }

            // BOTTOM edge: strip occupies y ∈ [rootH-bottom, rootH-1].
            // On a monitor whose bottom is NOT flush with the root bottom, the strut thickness
            // includes the gap below the monitor; the formula still lands on the panel's top.
            if (s.bottom > 0) {
                const int64_t thickness = clampToLimit(s.bottom, rootH);
                const auto span = resolveSpan(s.bottomStartX, s.bottomEndX, rootW);
                if (span && overlaps(rootH - thickness, rootH - 1, monT, monB) &&
                    overlaps(span->first, span->second, monL, monR)) {
                    workB = std::min(workB, rootH - thickness - 1);
                }
            }
        }
    }

    // Degenerate guard: a reservation that consumes the whole monitor is treated as bogus.
    if (workR < workL || workB < workT) {
        return monitor;
    }

    QRect result(QPoint(static_cast<int>(workL), static_cast<int>(workT)),
                 QPoint(static_cast<int>(workR), static_cast<int>(workB)));

    // Optional authoritative clip (single-head only, decided by the caller).
    if (netWorkArea && netWorkArea->isValid() && !netWorkArea->isEmpty()) {
        const QRect clipped = result.intersected(*netWorkArea);
        if (clipped.isValid() && !clipped.isEmpty()) {
            result = clipped;
        }
    }

    return result;
}
