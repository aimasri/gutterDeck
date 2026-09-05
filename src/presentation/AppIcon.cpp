#include "AppIcon.h"

#include <algorithm>
#include <vector>
#include <QColor>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>

namespace AppIcon {

QPixmap createBarcodePixmap(int size) {
    if (size <= 0) {
        size = 64;
    }

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    double scale = size / 256.0;
    p.scale(scale, scale);

    // 1. Dark sleek obsidian squircle container
    QRectF containerRect(10.0, 10.0, 236.0, 236.0);
    QPainterPath containerPath;
    containerPath.addRoundedRect(containerRect, 56.0, 56.0);

    QLinearGradient bgGrad(0.0, 0.0, 256.0, 256.0);
    bgGrad.setColorAt(0.0, QColor(0x18, 0x19, 0x24));
    bgGrad.setColorAt(1.0, QColor(0x0b, 0x0c, 0x12));
    p.fillPath(containerPath, bgGrad);

    // Subtle edge rim border
    p.strokePath(containerPath, QPen(QColor(255, 255, 255, 28), 2.5));

    // 2. Barcode lines with distinct vibrant colors and varying widths
    // Structure: (start_x, width, color, is_guard)
    struct BarcodeLine {
        double x;
        double w;
        QColor color;
        bool isGuard;
    };

    const std::vector<BarcodeLine> bars = {
        {32.0,  16.0, QColor(QStringLiteral("#06b6d4")), true},   // 1. Electric Cyan (Guard bar)
        {54.0,   9.0, QColor(QStringLiteral("#38bdf8")), false},  // 2. Sky Blue
        {68.0,  22.0, QColor(QStringLiteral("#6366f1")), false},  // 3. Vivid Indigo (Wide)
        {95.0,  11.0, QColor(QStringLiteral("#a855f7")), false},  // 4. Neon Purple
        {111.0, 15.0, QColor(QStringLiteral("#ec4899")), true},   // 5. Hot Pink (Center guard)
        {131.0, 25.0, QColor(QStringLiteral("#f43f5e")), false},  // 6. Coral Crimson (Wide)
        {161.0,  9.0, QColor(QStringLiteral("#fb923c")), false},  // 7. Bright Orange
        {175.0, 17.0, QColor(QStringLiteral("#eab308")), false},  // 8. Electric Gold
        {197.0, 10.0, QColor(QStringLiteral("#10b981")), false},  // 9. Emerald Green
        {212.0, 16.0, QColor(QStringLiteral("#00f2fe")), true},   // 10. Neon Mint (Guard bar)
    };

    for (const auto& bar : bars) {
        double t = bar.isGuard ? 36.0 : 46.0;
        double b = bar.isGuard ? 220.0 : 210.0;
        double h = b - t;
        double r = std::min(bar.w / 2.0, 5.0);

        QPainterPath barPath;
        barPath.addRoundedRect(QRectF(bar.x, t, bar.w, h), r, r);

        QLinearGradient g(bar.x, t, bar.x, b);
        g.setColorAt(0.0, bar.color.lighter(125));
        g.setColorAt(0.5, bar.color);
        g.setColorAt(1.0, bar.color.darker(105));
        p.fillPath(barPath, g);
    }

    p.end();
    return pix;
}

QIcon createAppIcon() {
    QIcon icon;
    const int sizes[] = {16, 24, 32, 48, 64, 128, 256};
    for (int sz : sizes) {
        icon.addPixmap(createBarcodePixmap(sz));
    }
    return icon;
}

} // namespace AppIcon
