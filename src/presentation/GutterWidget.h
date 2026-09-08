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

    [[nodiscard]] int index() const noexcept;
    [[nodiscard]] QString name() const;
    [[nodiscard]] QColor color() const noexcept;
    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] int gutterWidth() const noexcept;
    [[nodiscard]] SwellStage swellStage() const noexcept;
    [[nodiscard]] bool isLeftSide() const noexcept;

    void setActive(bool active);
    void setGutterWidth(int width);
    void setSwellStage(SwellStage stage);
    void setLeftSide(bool left);
    void setName(const QString& name);
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
     */
    void hoverEntered(int index);

    /**
     * @brief Emitted when mouse leaves this gutter tab.
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
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
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
