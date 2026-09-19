#pragma once

#include <QColor>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

enum class SwellStage {
    Collapsed,
    Halfway,
    Expanded
};

enum class SplitOrientation {
    Vertical,   ///< 50/50 left and right side-by-side split
    Horizontal  ///< 50/50 top and bottom stacked split
};
Q_DECLARE_METATYPE(SplitOrientation)

/**
 * @brief Interactive edge tab widget that swells on mouse hover.
 * @details Composited directly inside the OverlayWindow transparent canvas.
 *          Uses internal Qt raster engine animation on maximum/fixed width,
 *          bypassing X11 window manager geometry request throttling.
 * @note Emits clicked(int) when selected via mouse click or keyboard Enter/Space.
 */
class GutterWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int gutterWidth READ gutterWidth WRITE setGutterWidth)

public:
    static constexpr double kInactiveTopHeight = 80.0;
    /**
     * @brief Constructs a gutter widget for a specific deck index.
     * @param index Zero-based deck index.
     * @param name Display label for the deck.
     * @param color Background color and accent tint.
     * @param collapsedWidth Normal resting pixel width.
     * @param expandedWidth Expanded pixel width when hovered.
     * @param swellDurationMs Animation duration for hover swell.
     * @param parent Optional parent widget (OverlayWindow).
     */
    GutterWidget(
        int index,
        const QString& name,
        const QColor& color,
        int collapsedWidth,
        int expandedWidth,
        int swellDurationMs,
        QWidget* parent = nullptr
    );
    ~GutterWidget() override = default;

    GutterWidget(const GutterWidget&) = delete;
    GutterWidget& operator=(const GutterWidget&) = delete;
    GutterWidget(GutterWidget&&) = delete;
    GutterWidget& operator=(GutterWidget&&) = delete;

    /**
     * @brief Gets the zero-based deck index represented by this tab.
     * @return Integer deck index.
     */
    [[nodiscard]] int index() const noexcept;

    /**
     * @brief Gets the label text displayed on this tab.
     * @return Display name string.
     */
    [[nodiscard]] QString name() const;

    /**
     * @brief Gets the accent color of this tab.
     * @return QColor value.
     */
    [[nodiscard]] QColor color() const noexcept;

    /**
     * @brief Checks whether this deck is currently active and focused.
     * @return True if currently active.
     */
    [[nodiscard]] bool isActive() const noexcept;

    /**
     * @brief Gets the current animated width of the tab in pixels.
     * @return Pixel width.
     */
    [[nodiscard]] int gutterWidth() const noexcept;

    /**
     * @brief Gets the current swell animation stage (Collapsed, Halfway, Expanded).
     * @return SwellStage enum value.
     */
    [[nodiscard]] SwellStage swellStage() const noexcept;

    /**
     * @brief Checks whether the tab is positioned on the left side of the active deck.
     * @return True if positioned to the left.
     */
    [[nodiscard]] bool isLeftSide() const noexcept;

    /**
     * @brief Sets whether this deck is marked as active.
     * @param active True to mark active.
     */
    void setActive(bool active);

    /**
     * @brief Sets the pixel width of the gutter (invoked by QPropertyAnimation).
     * @param width New pixel width.
     */
    void setGutterWidth(int width);

    /**
     * @brief Sets the swell stage for visual styling.
     * @param stage SwellStage value.
     */
    void setSwellStage(SwellStage stage);

    /**
     * @brief Configures whether this gutter tab is anchored to the left or right of the screen.
     * @param left True for left side, false for right side.
     */
    void setLeftSide(bool left);

    /**
     * @brief Updates the tab label name.
     * @param name New display label.
     */
    void setName(const QString& name);

    /**
     * @brief Updates the tab color and triggers visual repaint.
     * @param color New accent color.
     */
    void setColor(const QColor& color);

signals:
    /**
     * @brief Emitted when the user activates this gutter via mouse or keyboard.
     * @param index Zero-based index of this deck.
     */
    void clicked(int index);

    /**
     * @brief Emitted when the user requests a split view with this gutter via Shift+Click (Vertical) or Ctrl+Click (Horizontal).
     * @param index Zero-based index of this deck.
     * @param orientation 50/50 vertical or horizontal split.
     */
    void splitRequested(int index, SplitOrientation orientation);

    /**
     * @brief Emitted when the user requests a context menu via right click.
     * @param index Zero-based index of this deck.
     * @param globalPos Global cursor position where clicked.
     */
    void contextMenuRequested(int index, const QPoint& globalPos);

    /**
     * @brief Emitted when the hover state changes (used to notify mask recalculation).
     */
    void hoverChanged();

    /**
     * @brief Emitted when mouse enters this gutter tab.
     * @param index Zero-based index of this deck.
     */
    void hoverEntered(int index);

    /**
     * @brief Emitted when mouse leaves this gutter tab.
     * @param index Zero-based index of this deck.
     */
    void hoverLeft(int index);

    /**
     * @brief Emitted when the user requests previous deck via left arrow or mouse back button.
     */
    void previousRequested();

    /**
     * @brief Emitted when the user requests next deck via right arrow or mouse forward button.
     */
    void nextRequested();

protected:
    /**
     * @brief Handles mouse cursor entry to initiate swell animation and emit hover signals.
     * @param event Pointer to enter event.
     */
    void enterEvent(QEnterEvent* event) override;

    /**
     * @brief Handles mouse cursor exit to collapse swell and emit leave signals.
     * @param event Pointer to leave event.
     */
    void leaveEvent(QEvent* event) override;

    /**
     * @brief Tracks cursor coordinate to distinguish inactive top-zone from interactive tab area.
     * @param event Pointer to mouse move event.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse clicks for deck switching, split-view requests, or context menus.
     * @param event Pointer to mouse press event.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Handles mouse wheel scrolling for cycling through deck tabs.
     * @param event Pointer to wheel event.
     */
    void wheelEvent(QWheelEvent* event) override;

    /**
     * @brief Handles keyboard navigation (Return/Space to activate, Arrow keys to navigate).
     * @param event Pointer to key event.
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief Renders the gutter tab background, border accent, and vertical rotated text.
     * @param event Pointer to paint event.
     */
    void paintEvent(QPaintEvent* event) override;

private:
    int m_index;
    QString m_name;
    QColor m_color;
    int m_collapsedWidth;
    int m_expandedWidth;
    int m_currentWidth;
    bool m_isActive = false;
    bool m_isHovered = false;
    bool m_isLeftSide = true;
    bool m_inInactiveZone = false;
    SwellStage m_stage = SwellStage::Collapsed;

    int m_wheelAccumulator = 0;
    QTimer m_wheelResetTimer;

    QPropertyAnimation m_swellAnimation;
};
