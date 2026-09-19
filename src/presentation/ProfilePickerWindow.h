#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QCheckBox;
class QVBoxLayout;
class QGridLayout;
struct ProfileInfo;

/**
 * @brief Dark-mode presentation window for profile selection, creation, and reordering.
 * @details Presents interactive profile cards with initials avatars and miniature deck
 *          palette preview bars. Operates either as the initial Phase 1 bootstrap picker in
 *          main() or as an on-the-fly in-dock profile manager triggered from DeckController.
 *          Encapsulates creation, renaming, color personalization, deletion, and sequence
 *          reordering with immediate configuration persistence.
 * @note Implements frameless drag handling and automatically centers on the screen
 *       beneath the user's mouse cursor during showEvent.
 */
class ProfilePickerWindow : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Constructs the frameless profile picker window.
     * @param parent Optional parent widget.
     */
    explicit ProfilePickerWindow(QWidget* parent = nullptr);

signals:
    /**
     * @brief Emitted when a user clicks a profile card to select it.
     * @param profileId Unique slug identifier of the selected profile.
     */
    void profileSelected(const QString& profileId);

    /**
     * @brief Emitted when the window is closed without an explicit profile selection.
     */
    void closedWithoutSelection();

protected:
    /**
     * @brief Handles Escape key press to dismiss the picker cleanly.
     * @param event QKeyEvent details.
     */
    void keyPressEvent(QKeyEvent* event) override;

    /**
     * @brief Handles window close to emit closedWithoutSelection if no profile was chosen.
     * @param event QCloseEvent details.
     */
    void closeEvent(QCloseEvent* event) override;

    /**
     * @brief Records initial mouse click offset for window reposition dragging.
     * @param event QMouseEvent details.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Moves the frameless window following mouse drag position.
     * @param event QMouseEvent details.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Auto-detects cursor screen and dynamically centers window on display.
     * @param event QShowEvent details.
     */
    void showEvent(QShowEvent* event) override;

private slots:
    /**
     * @brief Handles left-click on a profile card, emitting profileSelected.
     * @param profileId Target profile identifier.
     */
    void onProfileCardClicked(const QString& profileId);

    /**
     * @brief Opens sleek input and color dialogs to configure and create a new profile.
     */
    void onAddProfileClicked();

    /**
     * @brief Prompts for confirmation and deletes a profile, preventing deletion of the last remaining profile.
     * @param profileId Target profile identifier.
     */
    void onDeleteProfile(const QString& profileId);

    /**
     * @brief Prompts with sleek input dialog to rename an existing profile.
     * @param profileId Target profile identifier.
     */
    void onEditProfileName(const QString& profileId);

    /**
     * @brief Prompts with sleek color dialog to modify an existing profile's accent color.
     * @param profileId Target profile identifier.
     */
    void onEditProfileColor(const QString& profileId);

    /**
     * @brief Launches the modal SleekReorderDialog to arrange profiles in a custom sequence.
     */
    void onReorderProfilesClicked();

    /**
     * @brief Shifts a profile one position to the left (delta -1) or right (delta +1).
     * @param profileId Target profile identifier.
     * @param delta Movement delta (-1 for earlier, +1 for later).
     */
    void onMoveProfile(const QString& profileId, int delta);

private:
    /**
     * @brief Constructs header, card container layout, close button, and bottom actions.
     */
    void buildUI();

    /**
     * @brief Clears and repopulates profile cards in the grid layout based on ConfigManager registry.
     */
    void rebuildProfileCards();

    /**
     * @brief Creates a styled ProfileCard widget with initial avatar, title, and deck palette strip.
     * @param profile Metadata struct for the profile.
     * @param index Current zero-based index in the sequence.
     * @param totalCount Total number of profiles in registry.
     * @return Fully initialized card widget.
     */
    QWidget* createProfileCard(const ProfileInfo& profile, int index, int totalCount);

    /**
     * @brief Applies dark obsidian theme stylesheets.
     */
    void applyTheme();

    QWidget* m_cardsContainer = nullptr;
    QGridLayout* m_cardsLayout = nullptr;
    bool m_selected = false;
    QPoint m_dragPosition;
};
