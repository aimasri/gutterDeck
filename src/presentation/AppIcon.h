#pragma once

#include <QIcon>
#include <QPixmap>

namespace AppIcon {

/**
 * @brief Renders the multi-colored barcode style application icon.
 * @details Each vertical bar has a distinct vibrant hue on an obsidian rounded squircle.
 * @return QIcon bundled with multi-resolution pixmaps (16, 24, 32, 48, 64, 128, 256).
 */
QIcon createAppIcon();

/**
 * @brief Renders a single square pixmap of the barcode icon at the requested size.
 * @param size Target width and height in pixels.
 * @return Antialiased QPixmap containing the barcode icon.
 */
QPixmap createBarcodePixmap(int size);

} // namespace AppIcon
