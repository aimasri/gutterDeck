#include "GutterWidget.h"

#include <algorithm>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

GutterWidget::GutterWidget(
    int index,
    const QString& name,
    const QColor& color,
    int collapsedWidth,
    int expandedWidth,
    int swellDurationMs,
    QWidget* parent
) : QWidget(parent),
    m_index(index),
    m_name(name),
    m_color(color),
    m_collapsedWidth(collapsedWidth),
    m_expandedWidth(expandedWidth),
    m_currentWidth(collapsedWidth),
    m_swellAnimation(this, "gutterWidth") {

    setFocusPolicy(Qt::StrongFocus);
    setToolTip(name);
    setAccessibleName(name);
    setFixedWidth(m_collapsedWidth);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);

    m_swellAnimation.setDuration(swellDurationMs);
    m_swellAnimation.setEasingCurve(QEasingCurve::OutCubic);

    m_wheelResetTimer.setSingleShot(true);
    connect(&m_wheelResetTimer, &QTimer::timeout, this, [this]() {
        m_wheelAccumulator = 0;
    });
}

int GutterWidget::index() const noexcept {
    return m_index;
}

QString GutterWidget::name() const {
    return m_name;
}

QColor GutterWidget::color() const noexcept {
    return m_color;
}

bool GutterWidget::isActive() const noexcept {
    return m_isActive;
}

int GutterWidget::gutterWidth() const noexcept {
    return m_currentWidth;
}

void GutterWidget::setActive(bool active) {
    if (m_isActive != active) {
        m_isActive = active;
        update();
    }
}

void GutterWidget::setGutterWidth(int width) {
    m_currentWidth = width;
    setFixedWidth(width);
    update();
    emit hoverChanged();
}

void GutterWidget::setName(const QString& name) {
    m_name = name;
    setToolTip(name);
    setAccessibleName(name);
    update();
}

void GutterWidget::setColor(const QColor& color) {
    m_color = color;
    update();
}

bool GutterWidget::isLeftSide() const noexcept {
    return m_isLeftSide;
}

void GutterWidget::setLeftSide(bool left) {
    if (m_isLeftSide != left) {
        m_isLeftSide = left;
        update();
    }
}

SwellStage GutterWidget::swellStage() const noexcept {
    return m_stage;
}

void GutterWidget::setSwellStage(SwellStage stage) {
    if (m_stage == stage && m_swellAnimation.state() != QAbstractAnimation::Running) {
        return;
    }
    m_stage = stage;
    int targetWidth = m_collapsedWidth;
    if (stage == SwellStage::Halfway) {
        targetWidth = (m_collapsedWidth + m_expandedWidth) / 2;
    } else if (stage == SwellStage::Expanded) {
        targetWidth = m_expandedWidth;
    }

    m_isHovered = (stage == SwellStage::Expanded);

    m_swellAnimation.stop();
    m_swellAnimation.setStartValue(m_currentWidth);
    m_swellAnimation.setEndValue(targetWidth);
    m_swellAnimation.start();
}

void GutterWidget::enterEvent(QEnterEvent* event) {
    if (event->position().y() <= kInactiveTopHeight) {
        m_inInactiveZone = true;
        return;
    }
    m_inInactiveZone = false;
    emit hoverEntered(m_index);
}

void GutterWidget::leaveEvent(QEvent* event) {
    m_inInactiveZone = false;
    Q_UNUSED(event);
    emit hoverLeft(m_index);
}

void GutterWidget::mouseMoveEvent(QMouseEvent* event) {
    bool inInactiveZone = (event->position().y() <= kInactiveTopHeight);
    if (inInactiveZone) {
        if (m_stage != SwellStage::Collapsed && !m_inInactiveZone) {
            m_inInactiveZone = true;
            emit hoverLeft(m_index);
        }
    } else {
        m_inInactiveZone = false;
        if (m_stage != SwellStage::Expanded) {
            emit hoverEntered(m_index);
        }
    }
    QWidget::mouseMoveEvent(event);
}

void GutterWidget::mousePressEvent(QMouseEvent* event) {
    if (event->position().y() <= kInactiveTopHeight) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            emit splitRequested(m_index, SplitOrientation::Vertical);
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit splitRequested(m_index, SplitOrientation::Horizontal);
        } else {
            emit clicked(m_index);
        }
    } else if (event->button() == Qt::RightButton) {
        emit contextMenuRequested(m_index, event->globalPosition().toPoint());
    } else if (event->button() == Qt::BackButton) {
        emit previousRequested();
    } else if (event->button() == Qt::ForwardButton) {
        emit nextRequested();
    }
}

void GutterWidget::wheelEvent(QWheelEvent* event) {
    if (event->position().y() <= kInactiveTopHeight) {
        event->ignore();
        return;
    }

    int delta = event->angleDelta().y();
    if (delta == 0) {
        // Support horizontal tilt or trackpad horizontal gestures
        delta = -event->angleDelta().x();
    }

    if (delta == 0) {
        QWidget::wheelEvent(event);
        return;
    }

    // Reset accumulator on direction reversal
    if ((m_wheelAccumulator > 0 && delta < 0) || (m_wheelAccumulator < 0 && delta > 0)) {
        m_wheelAccumulator = 0;
    }

    m_wheelAccumulator += delta;
    m_wheelResetTimer.start(400);

    const int kStep = 120; // Standard single wheel detent
    if (m_wheelAccumulator >= kStep) {
        m_wheelAccumulator = 0;
        emit previousRequested();
    } else if (m_wheelAccumulator <= -kStep) {
        m_wheelAccumulator = 0;
        emit nextRequested();
    }

    event->accept();
}

void GutterWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Space) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            emit splitRequested(m_index, SplitOrientation::Vertical);
        } else if (event->modifiers().testFlag(Qt::ControlModifier)) {
            emit splitRequested(m_index, SplitOrientation::Horizontal);
        } else {
            emit clicked(m_index);
        }
    } else if (event->key() == Qt::Key_Left || event->key() == Qt::Key_A) {
        emit previousRequested();
    } else if (event->key() == Qt::Key_Right || event->key() == Qt::Key_S) {
        emit nextRequested();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void GutterWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 1. Draw base deck tint (boost resting opacity on razor-thin hairline gutter)
    QColor drawColor = m_color;
    if (!m_isHovered && m_currentWidth <= 6) {
        drawColor.setAlpha(std::max(180, m_color.alpha()));
    }
    painter.fillRect(rect(), drawColor);

    // 2. Subtle hover overlay
    if (m_isHovered) {
        painter.fillRect(rect(), QColor(255, 255, 255, 35));
    }

    // 3. Focus indication
    if (hasFocus()) {
        painter.setPen(QPen(Qt::white, 2, Qt::DotLine));
        painter.drawRect(rect().adjusted(1, 1, -1, -1));
    }

    // Unified anchor for active indicator and center icon
    QFont font("SansSerif", 9, QFont::Bold);
    QFontMetrics fm(font);
    QString icon = QString::fromUtf8("☰");
    int anchorY = height() / 2 - 25;
    int iconTopOffset = std::max(0, fm.tightBoundingRect(icon).x());

    // 4. Active deck indicator: fills 4px at rest, thins to 2px on hover facing the open window
    // Perfectly top-aligned with the center icon
    if (m_isActive) {
        int indWidth = (m_currentWidth <= 4) ? m_currentWidth : 2;
        if (indWidth > 0) {
            // Facing the open window in the center: right edge for left gutters, left edge for right gutters
            int indX = m_isLeftSide ? (width() - indWidth) : 0;
            int indHeight = 70;
            int indY = anchorY + iconTopOffset;
            painter.fillRect(QRect(indX, indY, indWidth, indHeight), Qt::white);
        }
    }

    // 5. Center icon and cascading text when expanded/halfway
    if (m_currentWidth >= 20) {
        painter.save();
        int centerY = anchorY;

        int activeIndW = (m_isActive && m_currentWidth > 4) ? 2 : 0;
        double centerX = m_isLeftSide
            ? (width() - activeIndW) / 2.0
            : activeIndW + (width() - activeIndW) / 2.0;

        painter.translate(centerX, centerY);
        painter.rotate(90);

        painter.setFont(font);
        painter.setPen(Qt::white);

        painter.drawText(0, fm.descent() + 2, icon);

        int iconAdvance = fm.horizontalAdvance(icon + QStringLiteral("  "));
        painter.drawText(iconAdvance, fm.descent() + 2, m_name);
        painter.restore();
    }
}
