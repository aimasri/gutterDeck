#include "OverlayWindow.h"
#include "GutterWidget.h"
#include "CurtainWidget.h"
#include "../domain/StateMachine.h"
#include "../infrastructure/ConfigManager.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QRegion>
#include <algorithm>

OverlayWindow::OverlayWindow(const ConfigManager& config, const QRect& screenGeometry,
                             QWidget* parent)
    : QWidget(parent),
      m_config(config),
      m_lastState(AppState::IDLE) {

    // Holy Grail overlay flags: Bypasses WM Z-ordering while staying permanently on top
    setWindowFlags(Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);

    m_leaveDebounceTimer.setSingleShot(true);
    connect(&m_leaveDebounceTimer, &QTimer::timeout,
            this, &OverlayWindow::onLeaveDebounceTimeout);

    setupLayout(screenGeometry);
    updateMask(AppState::IDLE);
}

void OverlayWindow::setupLayout(const QRect& screenGeometry) {
    setGeometry(screenGeometry);

    m_mainLayout = new QHBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    const auto& decks = m_config.getDecks();
    const auto& settings = m_config.getSettings();

    for (int i = 0; i < decks.size(); ++i) {
        auto* gutter = new GutterWidget(
            i,
            decks[i].name,
            decks[i].color,
            settings.gutterWidth,
            settings.gutterExpandedWidth,
            settings.swellDurationMs,
            this
        );
        m_gutters.append(gutter);
        wireGutter(gutter);
    }

    // Create child curtain widget parked offscreen
    m_curtain = new CurtainWidget(this);
    m_curtain->setGeometry(-10000, -10000, 1, 1);
    m_curtain->hide();

    rebuildAccordionLayout(0);
}

void OverlayWindow::rebuildAccordionLayout(int activeIndex) {
    m_currentActiveIndex = activeIndex;
    m_isSplitMode = false;
    m_splitLeftIndex = -1;
    m_splitRightIndex = -1;

    if (!m_mainLayout) {
        return;
    }

    // Detach current layout items safely without destroying child QWidgets
    QLayoutItem* item = nullptr;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    int total = m_gutters.size();
    if (total == 0) {
        return;
    }

    int active = std::max(0, std::min(activeIndex, total - 1));

    // 1. Left-side gutters (0..active)
    for (int i = 0; i <= active && i < total; ++i) {
        m_gutters[i]->setLeftSide(true);
        m_mainLayout->addWidget(m_gutters[i]);
    }

    // 2. Center stretch spacer where active deck application window is displayed
    m_mainLayout->addStretch(1);

    // 3. Right-side gutters (active+1..total-1)
    for (int i = active + 1; i < total; ++i) {
        m_gutters[i]->setLeftSide(false);
        m_mainLayout->addWidget(m_gutters[i]);
    }

    // 4. Update visual indicators
    for (int i = 0; i < total; ++i) {
        m_gutters[i]->setActive(i == activeIndex);
    }

    m_mainLayout->activate();
    updateMask(m_lastState);
}

void OverlayWindow::updateMask(AppState state) {
    m_lastState = state;

    if (state == AppState::IDLE) {
        // Idle: screen is transparent & click-through EXCEPT for physical gutter tab areas.
        // Full tab geometry is added to preserve entire tab rendering without top clipping.
        QRegion activeRegion;
        for (auto* gutter : m_gutters) {
            activeRegion += gutter->geometry();
        }
        if (activeRegion.isEmpty()) {
            activeRegion = QRegion(0, 0, 1, 1);
        }
        setMask(activeRegion);
    } else {
        // Switching: canvas is solid to capture and block all clicks while curtain moves
        clearMask();
    }
}

void OverlayWindow::setActiveGutter(int activeIndex) {
    rebuildAccordionLayout(activeIndex);
}

void OverlayWindow::enterSplitLayout(int leftDeckIndex, int rightDeckIndex, SplitOrientation orientation) {
    m_isSplitMode = true;
    m_splitLeftIndex = leftDeckIndex;
    m_splitRightIndex = rightDeckIndex;
    m_splitOrientation = orientation;
    m_currentActiveIndex = leftDeckIndex;

    if (!m_mainLayout) {
        return;
    }

    QLayoutItem* item = nullptr;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    int total = m_gutters.size();
    if (total == 0) {
        return;
    }

    // 1. Left-side gutters (0..leftDeckIndex)
    for (int i = 0; i <= leftDeckIndex && i < total; ++i) {
        m_gutters[i]->setLeftSide(true);
        m_mainLayout->addWidget(m_gutters[i]);
    }

    // 2. Center stretch spacer where active split application windows are displayed
    m_mainLayout->addStretch(1);

    // 3. Right-side gutters (rightDeckIndex..total-1)
    for (int i = rightDeckIndex; i < total; ++i) {
        m_gutters[i]->setLeftSide(false);
        m_mainLayout->addWidget(m_gutters[i]);
    }

    // 4. Update visual indicators: both decks in the split are marked active
    for (int i = 0; i < total; ++i) {
        m_gutters[i]->setActive(i == leftDeckIndex || i == rightDeckIndex);
    }

    m_mainLayout->activate();
    updateMask(m_lastState);
}

void OverlayWindow::exitSplitLayout(int activeIndex) {
    m_isSplitMode = false;
    m_splitLeftIndex = -1;
    m_splitRightIndex = -1;
    rebuildAccordionLayout(activeIndex);
}

bool OverlayWindow::isSplitMode() const noexcept {
    return m_isSplitMode;
}

void OverlayWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    updateMask(m_lastState);
}

void OverlayWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateMask(m_lastState);
}

void OverlayWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Clear);
    painter.fillRect(rect(), Qt::transparent);
}

void OverlayWindow::onGutterHoverChanged() {
    if (m_lastState == AppState::IDLE) {
        updateMask(AppState::IDLE);
    }
}

void OverlayWindow::onGutterHoverEntered(int index) {
    m_leaveDebounceTimer.stop();

    int total = m_gutters.size();
    if (index < 0 || index >= total) {
        return;
    }

    // Determine side: in split mode, left side is 0..m_splitLeftIndex; otherwise 0..m_currentActiveIndex
    bool isLeftSide = m_isSplitMode ? (index <= m_splitLeftIndex) : (index <= m_currentActiveIndex);

    for (int i = 0; i < total; ++i) {
        bool sameSide = m_isSplitMode ? ((i <= m_splitLeftIndex) == isLeftSide)
                                      : ((i <= m_currentActiveIndex) == isLeftSide);
        if (sameSide) {
            if (i == index) {
                m_gutters[i]->setSwellStage(SwellStage::Expanded);
            } else {
                m_gutters[i]->setSwellStage(SwellStage::Halfway);
            }
        } else {
            m_gutters[i]->setSwellStage(SwellStage::Collapsed);
        }
    }
}

void OverlayWindow::onGutterHoverLeft(int index) {
    Q_UNUSED(index);
    m_leaveDebounceTimer.start(40);
}

void OverlayWindow::onLeaveDebounceTimeout() {
    if (m_hoverLock) return;

    bool isHovering = false;
    for (auto* gutter : m_gutters) {
        if (gutter->underMouse()) {
            isHovering = true;
            break;
        }
    }

    if (isHovering) return;

    for (auto* gutter : m_gutters) {
        gutter->setSwellStage(SwellStage::Collapsed);
    }
}

CurtainWidget* OverlayWindow::curtain() const noexcept {
    return m_curtain;
}

const QVector<GutterWidget*>& OverlayWindow::gutters() const noexcept {
    return m_gutters;
}

void OverlayWindow::wireGutter(GutterWidget* gutter) {
    connect(gutter, &GutterWidget::hoverChanged,
            this, &OverlayWindow::onGutterHoverChanged);
    connect(gutter, &GutterWidget::hoverEntered,
            this, &OverlayWindow::onGutterHoverEntered);
    connect(gutter, &GutterWidget::hoverLeft,
            this, &OverlayWindow::onGutterHoverLeft);
    connect(gutter, &GutterWidget::clicked,
            this, &OverlayWindow::gutterClicked);
    connect(gutter, &GutterWidget::splitRequested,
            this, &OverlayWindow::splitRequested);
    connect(gutter, &GutterWidget::previousRequested,
            this, &OverlayWindow::previousRequested);
    connect(gutter, &GutterWidget::nextRequested,
            this, &OverlayWindow::nextRequested);
    connect(gutter, &GutterWidget::contextMenuRequested,
            this, &OverlayWindow::contextMenuRequested);
}

void OverlayWindow::updateGutterVisuals(int index, const QString& name, const QColor& color) {
    if (index >= 0 && index < m_gutters.size()) {
        m_gutters[index]->setName(name);
        m_gutters[index]->setColor(color);
        update();
    }
}

void OverlayWindow::rebuildGutters() {
    if (!m_mainLayout) {
        return;
    }

    // 1. Remove widgets from layout safely
    QLayoutItem* item = nullptr;
    while ((item = m_mainLayout->takeAt(0)) != nullptr) {
        delete item;
    }

    // 2. Hide and delete existing gutter instances
    for (auto* gutter : m_gutters) {
        gutter->hide();
        gutter->deleteLater();
    }
    m_gutters.clear();

    // 3. Create new gutters from updated config
    const auto& decks = m_config.getDecks();
    const auto& settings = m_config.getSettings();

    for (int i = 0; i < decks.size(); ++i) {
        auto* gutter = new GutterWidget(
            i,
            decks[i].name,
            decks[i].color,
            settings.gutterWidth,
            settings.gutterExpandedWidth,
            settings.swellDurationMs,
            this
        );
        gutter->show();
        m_gutters.append(gutter);
        wireGutter(gutter);
    }

    // 4. Reconstruct layout and refresh mask
    rebuildAccordionLayout(m_currentActiveIndex);
}

void OverlayWindow::setHoverLock(bool locked) {
    m_hoverLock = locked;
    if (!m_hoverLock) {
        onLeaveDebounceTimeout();
    }
}
