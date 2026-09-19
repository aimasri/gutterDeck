#pragma once

#include <QRect>
#include <QRegion>
#include <QTimer>
#include <QVector>
#include <QWidget>

class ConfigManager;
#include "GutterWidget.h"
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
     * @brief Locks or unlocks gutter hover swell animations during context menus and modal dialogs.
     * @param locked True to freeze hover states and prevent accidental trigger wipes.
     */
    void setHoverLock(bool locked);

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
     * @return Non-owning pointer to CurtainWidget.
     */
    [[nodiscard]] CurtainWidget* curtain() const noexcept;

    /**
     * @brief Accessor to all managed gutter child widgets.
     * @return Const reference to QVector of GutterWidget pointers.
     */
    [[nodiscard]] const QVector<GutterWidget*>& gutters() const noexcept;

    /**
     * @brief Reconstructs all gutter widgets when decks are added or removed.
     */
    void rebuildGutters();

    /**
     * @brief Updates visuals for a specific deck tab without rebuilding layout.
     * @param index Target deck index.
     * @param name Updated display name.
     * @param color Updated tab color.
     */
    void updateGutterVisuals(int index, const QString& name, const QColor& color);

    /**
     * @brief Rebuilds accordion layout for split view with two active deck tabs.
     * @param leftDeckIndex Index of left/top deck in split.
     * @param rightDeckIndex Index of right/bottom deck in split.
     * @param orientation Vertical or Horizontal split.
     */
    void enterSplitLayout(int leftDeckIndex, int rightDeckIndex, SplitOrientation orientation);

    /**
     * @brief Restores normal single-active-deck accordion layout.
     * @param activeIndex Index of newly focused deck.
     */
    void exitSplitLayout(int activeIndex);

    /**
     * @brief Queries whether the overlay is currently in split layout mode.
     * @return True if in split view mode.
     */
    [[nodiscard]] bool isSplitMode() const noexcept;

signals:
    /**
     * @brief Emitted when a gutter tab is clicked.
     * @param index Zero-based index of the clicked gutter.
     */
    void gutterClicked(int index);

    /**
     * @brief Emitted when a split view transition is requested with an adjacent deck.
     * @param index Zero-based index of target adjacent deck.
     * @param orientation Vertical or Horizontal split.
     */
    void splitRequested(int index, SplitOrientation orientation);

    /**
     * @brief Emitted when previous deck navigation is triggered.
     */
    void previousRequested();

    /**
     * @brief Emitted when next deck navigation is triggered.
     */
    void nextRequested();

    /**
     * @brief Emitted when a user right-clicks a gutter tab to open the context menu.
     * @param index Zero-based index of right-clicked gutter.
     * @param globalPos Global screen cursor position.
     */
    void contextMenuRequested(int index, const QPoint& globalPos);

protected:
    /**
     * @brief Refreshes XShape mask when window is shown.
     * @param event QShowEvent details.
     */
    void showEvent(QShowEvent* event) override;

    /**
     * @brief Recalculates layout bounds on screen resize.
     * @param event QResizeEvent details.
     */
    void resizeEvent(QResizeEvent* event) override;

    /**
     * @brief Clears background to pure transparency.
     * @param event QPaintEvent details.
     */
    void paintEvent(QPaintEvent* event) override;

private slots:
    /**
     * @brief Invoked during gutter width animations to dynamically update the XShape mask.
     */
    void onGutterHoverChanged();

    /**
     * @brief Invoked when mouse cursor enters a specific gutter.
     * @param index Deck index hovered.
     */
    void onGutterHoverEntered(int index);

    /**
     * @brief Invoked when mouse cursor leaves a specific gutter.
     * @param index Deck index unhovered.
     */
    void onGutterHoverLeft(int index);
    void onLeaveDebounceTimeout();

private:
    bool m_hoverLock = false;
    const ConfigManager& m_config;
    class QHBoxLayout* m_mainLayout = nullptr;
    CurtainWidget* m_curtain = nullptr;
    QVector<GutterWidget*> m_gutters;
    AppState m_lastState;
    int m_currentActiveIndex = 0;
    bool m_isSplitMode = false;
    int m_splitLeftIndex = -1;
    int m_splitRightIndex = -1;
    SplitOrientation m_splitOrientation = SplitOrientation::Vertical;
    QTimer m_leaveDebounceTimer;

    void setupLayout(const QRect& screenGeometry);
    void rebuildAccordionLayout(int activeIndex);
    void wireGutter(GutterWidget* gutter);
};
