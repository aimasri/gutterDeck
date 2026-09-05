#pragma once

#include <QColor>
#include <QPropertyAnimation>
#include <QRect>
#include <QWidget>

/**
 * @brief Transition curtain widget used to wipe across the screen during deck swaps.
 * @details Lives as a child widget within the full-screen OverlayWindow.
 *          Masks the visual artifacts of X11 window minimization and activation.
 * @note Emits slideComplete() upon finishing an animation pass.
 */
class CurtainWidget : public QWidget {
    Q_OBJECT
public:
    explicit CurtainWidget(QWidget* parent = nullptr);
    ~CurtainWidget() override = default;

    CurtainWidget(const CurtainWidget&) = delete;
    CurtainWidget& operator=(const CurtainWidget&) = delete;
    CurtainWidget(CurtainWidget&&) = delete;
    CurtainWidget& operator=(CurtainWidget&&) = delete;

    /**
     * @brief Initiates Phase 1: curtain slides into view, covering the desktop.
     * @param from Starting geometry (typically off-screen).
     * @param to Target geometry (typically full screen).
     * @param durationMs Animation duration in milliseconds.
     */
    void slideIn(const QRect& from, const QRect& to, int durationMs);

    /**
     * @brief Initiates Phase 2: curtain slides out of view, revealing new deck.
     * @param from Starting geometry (typically full screen).
     * @param to Target geometry (typically off-screen opposite side).
     * @param durationMs Animation duration in milliseconds.
     */
    void slideOut(const QRect& from, const QRect& to, int durationMs);

    /**
     * @brief Sets the solid background fill color of the curtain.
     */
    void setColor(const QColor& color);

signals:
    /**
     * @brief Emitted when either slideIn or slideOut finishes animating.
     */
    void slideComplete();

protected:
    void paintEvent(QPaintEvent* event) override;

private slots:
    void onAnimationFinished();

private:
    QColor m_color{Qt::black};
    QPropertyAnimation m_animation;
    bool m_isSlidingOut = false;
};
