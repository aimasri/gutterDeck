#include "CurtainWidget.h"

#include <QPainter>

CurtainWidget::CurtainWidget(QWidget* parent)
    : QWidget(parent),
      m_animation(this, "geometry") {

    m_animation.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_animation, &QPropertyAnimation::finished,
            this, &CurtainWidget::onAnimationFinished);

    setGeometry(-10000, -10000, 1, 1);
    hide();
}

void CurtainWidget::slideIn(const QRect& from, const QRect& to, int durationMs) {
    m_isSlidingOut = false;
    m_animation.stop();
    setGeometry(from);
    show();
    raise();

    m_animation.setDuration(durationMs);
    m_animation.setStartValue(from);
    m_animation.setEndValue(to);
    m_animation.start();
}

void CurtainWidget::slideOut(const QRect& from, const QRect& to, int durationMs) {
    m_isSlidingOut = true;
    m_animation.stop();
    setGeometry(from);
    show();
    raise();

    m_animation.setDuration(durationMs);
    m_animation.setStartValue(from);
    m_animation.setEndValue(to);
    m_animation.start();
}

void CurtainWidget::setColor(const QColor& color) {
    m_color = color;
    update();
}

void CurtainWidget::onAnimationFinished() {
    if (m_isSlidingOut) {
        hide();
        setGeometry(-10000, -10000, 1, 1);
    }
    emit slideComplete();
}

void CurtainWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), m_color);
}
