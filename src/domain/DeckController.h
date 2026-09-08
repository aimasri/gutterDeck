#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVector>
#include <xcb/xcb.h>

#include "../presentation/GutterWidget.h"

class StateMachine;
class XcbEngine;
class WindowWatcher;
class AppLauncher;
class ConfigManager;
class OverlayWindow;

/**
 * @brief Represents runtime state and X11 tracking for an individual deck.
 */
struct DeckSlot {
    QString id;
    QString name;
    QString command;
    QColor color;
    qint64 pid = 0;
    xcb_window_t windowId = XCB_WINDOW_NONE;
    bool isMapped = false;
};

/**
 * @brief High-level domain orchestrator for deck switching and window binding.
 * @details Couples the user presentation layer (gutters, curtain) with low-level
 *          X11 manipulations (minimize, restore, configure) governed by the StateMachine.
 *          Handles PID-to-window matching from AppLauncher and WindowWatcher.
 * @note Replaces the monolithic God Object GutterDeckApp.
 */
class DeckController : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs controller with constructor-injected subsystem dependencies.
     */
    DeckController(
        StateMachine& stateMachine,
        XcbEngine& xcbEngine,
        WindowWatcher& windowWatcher,
        AppLauncher& appLauncher,
        ConfigManager& config,
        QObject* parent = nullptr
    );
    ~DeckController() override;

    DeckController(const DeckController&) = delete;
    DeckController& operator=(const DeckController&) = delete;
    DeckController(DeckController&&) = delete;
    DeckController& operator=(DeckController&&) = delete;

    /**
     * @brief Sets non-owning observer pointer to the presentation overlay window.
     */
    void setOverlay(OverlayWindow* overlay) noexcept;

    /**
     * @brief Initializes deck configuration, connects watcher/launcher signals,
     *        and triggers application launches.
     */
    void initialize();

    /**
     * @brief Gets currently active deck index.
     */
    [[nodiscard]] int activeDeckIndex() const noexcept;

    /**
     * @brief Gets immutable access to deck slots.
     */
    [[nodiscard]] const QVector<DeckSlot>& decks() const noexcept;

    /**
     * @brief Returns true if controller is currently in 50/50 split view mode.
     */
    [[nodiscard]] bool isSplitMode() const noexcept;

    /**
     * @brief Gets current split orientation (Vertical or Horizontal).
     */
    [[nodiscard]] SplitOrientation splitOrientation() const noexcept;

    /**
     * @brief Gets the first (left or top) deck index in the active split view.
     */
    [[nodiscard]] int splitPrimaryIndex() const noexcept;

    /**
     * @brief Gets the second (right or bottom) deck index in the active split view.
     */
    [[nodiscard]] int splitSecondaryIndex() const noexcept;

    /**
     * @brief Calculates geometry for the primary (left or top) split window.
     */
    [[nodiscard]] QRect splitPrimaryGeometry() const;

    /**
     * @brief Calculates geometry for the secondary (right or bottom) split window.
     */
    [[nodiscard]] QRect splitSecondaryGeometry() const;

public slots:
    /**
     * @brief Triggered when a user clicks a gutter tab.
     * @param index Zero-based index of the clicked gutter.
     */
    void onGutterClicked(int index);

    /**
     * @brief Triggered when a user requests a split view with an adjacent deck.
     * @param index Zero-based index of the target gutter to split with.
     * @param orientation Vertical (side-by-side) or Horizontal (top/bottom).
     */
    void onSplitRequested(int index, SplitOrientation orientation);

    /**
     * @brief Exits split view mode, expanding one deck to fullscreen and minimizing the other.
     * @param focusDeckIndex Deck index to focus fullscreen (defaults to primary if -1).
     */
    void exitSplitMode(int focusDeckIndex = -1);

    /**
     * @brief Triggered when AppLauncher spawns a deck command.
     */
    void onDeckLaunched(int index, qint64 pid, const QString& command);

    /**
     * @brief Triggered by WindowWatcher when a top-level X11 window maps.
     */
    void onWindowMapped(uint32_t wid, uint32_t pid, const QString& title);

    /**
     * @brief Triggered by WindowWatcher when an X11 window is unmapped or destroyed.
     */
    void onWindowDestroyed(uint32_t wid);

    /**
     * @brief Triggered by WindowWatcher when the active X11 desktop workspace changes.
     * @param currentDesktop Zero-based index of newly active workspace.
     */
    void onCurrentDesktopChanged(uint32_t currentDesktop);

    /**
     * @brief Gets the assigned workspace desktop for this Gutter Deck instance.
     */
    [[nodiscard]] uint32_t assignedDesktop() const noexcept;

    /**
     * @brief Switches to the previous deck in sequence.
     * @param wrap If true, wrapping past deck 0 returns to the last deck. Defaults to false.
     */
    void switchToPreviousDeck(bool wrap = false);

    /**
     * @brief Switches to the next deck in sequence.
     * @param wrap If true, wrapping past the last deck returns to deck 0. Defaults to false.
     */
    void switchToNextDeck(bool wrap = false);

    /**
     * @brief Triggered when a user right-clicks a gutter tab to open context menu.
     * @param index Zero-based index of the right-clicked gutter.
     * @param globalPos Global screen position of mouse click.
     */
    void onGutterContextMenuRequested(int index, const QPoint& globalPos);

    void onEditDeckName(int index, const QPoint& customCenter = QPoint());
    void onEditDeckCommand(int index, const QPoint& customCenter = QPoint());
    void onChangeDeckColor(int index, const QPoint& customCenter = QPoint());
    void onAddNewDeck(int relativeToIndex = -1, const QPoint& customCenter = QPoint());
    void onReorderDecks();
    void onCloseDeck(int index);
    void closeGutterDeck();

signals:
    /**
     * @brief Emitted when a deck transition finishes successfully.
     * @param newActiveDeck The newly visible deck index.
     */
    void deckSwitched(int newActiveDeck);

    /**
     * @brief Emitted when split view mode is entered or exited.
     */
    void splitModeChanged(bool active);

    /**
     * @brief Emitted when an attached deck window is closed externally.
     */
    void deckClosed(int deckIndex);

private:
    void enterSplitMode(int firstDeck, int secondDeck, SplitOrientation orientation);
    void performSwitch(int targetDeck);
    void onCurtainPhase1Complete(int targetDeck);
    void onSwitchComplete();
    [[nodiscard]] QRect targetGeometry() const;

    StateMachine& m_stateMachine;
    XcbEngine& m_xcbEngine;
    WindowWatcher& m_windowWatcher;
    AppLauncher& m_appLauncher;
    ConfigManager& m_config;
    OverlayWindow* m_overlay = nullptr;

    QVector<DeckSlot> m_decks;
    int m_activeDeckIndex = -1;
    bool m_isSplitMode = false;
    SplitOrientation m_splitOrientation = SplitOrientation::Vertical;
    int m_splitPrimaryIndex = -1;
    int m_splitSecondaryIndex = -1;
    bool m_slideFromRight = true;
    uint32_t m_assignedDesktop = 0;
    int m_lastLaunchedDeckIndex = -1;
    qint64 m_lastLaunchedTimestampMs = 0;
};
