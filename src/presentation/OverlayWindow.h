#pragma once

#include <QRect>
#include <QRegion>
#include <QTimer>
#include <QVector>
#include <QWidget>

class ConfigManager;
class GutterWidget;
class CurtainWidget;
enum class AppState;

/**
 * @brief Full-screen transparent overlay window hosting gutters and curtain transition.
 * @details Implements the "Holy Grail" architecture: a single transparent X11 window
 *          with Qt::X11BypassWindowManagerHint. Child widgets are rendered using Qt's
 *          internal software/raster engine, bypassing X11 geometry request throttling.
 *
 *          Implements dynamic X11 Shape Extension click-through masking:
 *          - IDLE: Mask matches visible gutter tabs; rest of desktop is completely click-through.
 *          - SWITCHING: Mask is cleared, making the window solid to absorb all clicks during wipes.
 * @note Replaces the monolithic UI handling previously in GutterDeckApp.
 */
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs overlay window sized to screen geometry.
     * @param config Reference to application configuration.
     * @param screenGeometry Dimensions of the target display.
     * @param parent Optional parent widget.
     */
    OverlayWindow(const ConfigManager& config, const QRect& screenGeometry,
                  QWidget* parent = nullptr);
    ~OverlayWindow() override = default;

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;
    OverlayWindow(OverlayWindow&&) = delete;
    OverlayWindow& operator=(OverlayWindow&&) = delete;

    /**
     * @brief Recalculates and applies X11 XShape mask based on state and gutter geometries.
     * @param state Current AppState from StateMachine.
     */
    void updateMask(AppState state);

    /**
     * @brief Updates visual active indicators across all gutter widgets.
     * @param activeIndex Index of the active deck.
     */
    void setActiveGutter(int activeIndex);

    /**
     * @brief Accessor to the child curtain transition widget.
     */
    [[nodiscard]] CurtainWidget* curtain() const noexcept;

    /**
     * @brief Accessor to all managed gutter child widgets.
     */
    [[nodiscard]] const QVector<GutterWidget*>& gutters() const noexcept;

    /**
     * @brief Reconstructs all gutter widgets when decks are added or removed.
     */
    void rebuildGutters();

    /**
     * @brief Updates visuals for a specific deck tab without rebuilding layout.
     */
    void updateGutterVisuals(int index, const QString& name, const QColor& color);

signals:
    void gutterClicked(int index);
    void previousRequested();
    void nextRequested();
    void contextMenuRequested(int index, const QPoint& globalPos);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onGutterHoverChanged();
    void onGutterHoverEntered(int index);
    void onGutterHoverLeft(int index);
    void onLeaveDebounceTimeout();

private:
    const ConfigManager& m_config;
    class QHBoxLayout* m_mainLayout = nullptr;
    CurtainWidget* m_curtain = nullptr;
    QVector<GutterWidget*> m_gutters;
    AppState m_lastState;
    int m_currentActiveIndex = 0;
    QTimer m_leaveDebounceTimer;

    void setupLayout(const QRect& screenGeometry);
    void rebuildAccordionLayout(int activeIndex);
    void wireGutter(GutterWidget* gutter);
};
