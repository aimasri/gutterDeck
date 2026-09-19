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
 * @details Stores the configuration identity, window manager title, shell launch command,
 *          and color palette alongside live OS tracking data (process ID, native X11 window ID,
 *          and mapping status).
 * @note Managed and manipulated by DeckController; values are updated when WindowWatcher detects X11 events.
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
     * @param stateMachine Reference to the animation StateMachine.
     * @param xcbEngine Reference to the X11 XCB engine.
     * @param windowWatcher Reference to the event-driven WindowWatcher.
     * @param appLauncher Reference to the AppLauncher service.
     * @param config Reference to configuration repository.
     * @param parent Optional Qt parent.
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
     * @param overlay Pointer to active OverlayWindow instance.
     */
    void setOverlay(OverlayWindow* overlay) noexcept;

    /**
     * @brief Initializes deck configuration, connects watcher/launcher signals,
     *        and triggers application launches.
     */
    void initialize();

    /**
     * @brief Gets currently active deck index.
     * @return Zero-based index of the currently active deck, or -1 if none.
     */
    [[nodiscard]] int activeDeckIndex() const noexcept;

    /**
     * @brief Gets immutable access to deck slots.
     * @return Const reference to QVector of DeckSlot objects.
     */
    [[nodiscard]] const QVector<DeckSlot>& decks() const noexcept;

    /**
     * @brief Returns true if controller is currently in 50/50 split view mode.
     * @return True if split view is currently engaged.
     */
    [[nodiscard]] bool isSplitMode() const noexcept;

    /**
     * @brief Gets current split orientation (Vertical or Horizontal).
     * @return SplitOrientation enum value.
     */
    [[nodiscard]] SplitOrientation splitOrientation() const noexcept;

    /**
     * @brief Gets the first (left or top) deck index in the active split view.
     * @return Deck index, or -1 if not in split mode.
     */
    [[nodiscard]] int splitPrimaryIndex() const noexcept;

    /**
     * @brief Gets the second (right or bottom) deck index in the active split view.
     * @return Deck index, or -1 if not in split mode.
     */
    [[nodiscard]] int splitSecondaryIndex() const noexcept;

    /**
     * @brief Calculates geometry for the primary (left or top) split window.
     * @return QRect representing bounding box.
     */
    [[nodiscard]] QRect splitPrimaryGeometry() const;

    /**
     * @brief Calculates geometry for the secondary (right or bottom) split window.
     * @return QRect representing bounding box.
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
     * @param index Zero-based index of launched deck.
     * @param pid Process ID assigned by OS.
     * @param command Shell command executed.
     */
    void onDeckLaunched(int index, qint64 pid, const QString& command);

    /**
     * @brief Triggered by WindowWatcher when a top-level X11 window maps.
     * @param wid The native X11 window ID.
     * @param pid Process ID associated with the window (_NET_WM_PID).
     * @param title Window title text.
     */
    void onWindowMapped(uint32_t wid, uint32_t pid, const QString& title);

    /**
     * @brief Triggered by WindowWatcher when an X11 window is unmapped or destroyed.
     * @param wid The native X11 window ID.
     */
    void onWindowDestroyed(uint32_t wid);

    /**
     * @brief Triggered by WindowWatcher when the active X11 desktop workspace changes.
     * @param currentDesktop Zero-based index of newly active workspace.
     */
    void onCurrentDesktopChanged(uint32_t currentDesktop);

    /**
     * @brief Gets the assigned workspace desktop for this Gutter Deck instance.
     * @return Zero-based virtual desktop workspace index.
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

    /**
     * @brief Opens sleek input dialog to edit a deck slot label.
     * @param index Target deck index.
     * @param customCenter Optional point to center the dialog on.
     */
    void onEditDeckName(int index, const QPoint& customCenter = QPoint());

    /**
     * @brief Opens sleek input dialog to modify a deck slot launch command.
     * @param index Target deck index.
     * @param customCenter Optional point to center the dialog on.
     */
    void onEditDeckCommand(int index, const QPoint& customCenter = QPoint());

    /**
     * @brief Opens sleek color dialog to update deck tab color and opacity.
     * @param index Target deck index.
     * @param customCenter Optional point to center the dialog on.
     */
    void onChangeDeckColor(int index, const QPoint& customCenter = QPoint());

    /**
     * @brief Prompts with sleek dialog to create a new deck slot adjacent to a given index.
     * @param relativeToIndex Reference index to insert next to.
     * @param customCenter Optional point to center the dialog on.
     */
    void onAddNewDeck(int relativeToIndex = -1, const QPoint& customCenter = QPoint());

    /**
     * @brief Opens sleek modal reorder dialog to reorder deck tabs.
     */
    void onReorderDecks();

    /**
     * @brief Hands off execution to a different profile, gracefully closing windows.
     * @param targetProfileId Identifier of profile to launch.
     */
    void onSwitchProfile(const QString& targetProfileId);

    /**
     * @brief Opens the modal ProfilePickerWindow from within the active dock session.
     */
    void onOpenProfileManager();

    /**
     * @brief Gracefully closes an individual deck and removes it from the configuration.
     * @param index Target deck index to close.
     */
    void onCloseDeck(int index);

    /**
     * @brief Gracefully closes all managed deck windows and terminates the application.
     */
    void closeGutterDeck();

signals:
    /**
     * @brief Emitted when a deck transition finishes successfully.
     * @param newActiveDeck The newly visible deck index.
     */
    void deckSwitched(int newActiveDeck);

    /**
     * @brief Emitted when split view mode is entered or exited.
     * @param active True if split mode is now active; false if single fullscreen.
     */
    void splitModeChanged(bool active);

    /**
     * @brief Emitted when an attached deck window is closed externally.
     * @param deckIndex The index of the closed deck.
     */
    void deckClosed(int deckIndex);

private:
    /**
     * @brief Transitions layout and X11 window bounds into split view.
     * @param firstDeck Left/top deck index.
     * @param secondDeck Right/bottom deck index.
     * @param orientation Orientation of the split layout.
     */
    void enterSplitMode(int firstDeck, int secondDeck, SplitOrientation orientation);

    /**
     * @brief Orchestrates two-phase curtain animation and X11 window state switching.
     * @param targetDeck Zero-based index of incoming target deck.
     */
    void performSwitch(int targetDeck);

    /**
     * @brief Callback invoked when Phase 1 curtain wipe completes covering the screen.
     * @param targetDeck Zero-based index of incoming target deck.
     */
    void onCurtainPhase1Complete(int targetDeck);

    /**
     * @brief Callback invoked when Phase 2 curtain wipe completes revealing the new deck.
     */
    void onSwitchComplete();

    /**
     * @brief Calculates current screen or overlay geometry for window positioning.
     * @return Target screen QRect.
     */
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
