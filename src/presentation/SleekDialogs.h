#pragma once

#include <QColor>
#include <QDialog>
#include <QMenu>
#include <QString>
#include <QVector>

class QLineEdit;
class QSlider;
class QLabel;
class QPushButton;
class QRadioButton;
class QListWidget;

/**
 * @brief Metadata for displaying and reordering deck items in dialogs.
 * @details Encapsulates slot identity, display label, underlying command string,
 *          and assigned theme color for transfer between DeckController and dialog UI.
 * @note Struct is a plain data holder; does not take ownership of any system resources.
 */
struct DeckItemInfo {
    int originalIndex = 0;
    QString name;
    QString command;
    QColor color;
};

/**
 * @brief Generic item metadata for SleekReorderDialog.
 * @details Represents an arbitrary ordered entity (e.g. deck slots, profiles)
 *          with an identifier, display label, and optional accent color.
 * @note Decouples the reordering UI from deck-specific domain models.
 */
struct ReorderableItem {
    int originalIndex = 0;
    QString id;
    QString label;
    QColor color;
};

/**
 * @brief Identifiers for vector icons rendered in sleek context menus.
 */
enum class SleekMenuIcon {
    EditName,
    EditCommand,
    ChangeColor,
    AddDeck,
    ReorderDecks,
    DeleteDeck,
    CloseApp,
    SplitVertical,
    SplitHorizontal,
    ExitSplit
};

/**
 * @brief Generates an antialiased QIcon for the requested menu action.
 * @param type SleekMenuIcon enum selecting the glyph design.
 * @return Styled QIcon instance suitable for QMenu item display.
 */
[[nodiscard]] QIcon getSleekMenuIcon(SleekMenuIcon type);

/**
 * @brief Returns the curated 24-color dark-mode palette organized across blues, greens, reds, and oranges.
 * @return Constant reference to a vector of hex color code strings.
 */
[[nodiscard]] const QVector<QString>& getSleekDarkPalette();

/**
 * @brief Sleek, dark minimalist context menu for gutter tabs.
 * @details Implements custom dark obsidian styling with frameless edges,
 *          accented hover states, and iconography matching the GutterDeck visual identity.
 * @note Relies on Qt parent-child hierarchy for automatic memory reclamation.
 */
class SleekContextMenu : public QMenu {
    Q_OBJECT
public:
    /**
     * @brief Constructs a sleek context menu with embedded dark stylesheets.
     * @param parent Optional parent widget.
     */
    explicit SleekContextMenu(QWidget* parent = nullptr);
};

/**
 * @brief Minimalist modal dialog for text editing (Deck Name, Launch Command).
 * @details Provides a focused, single-input modal dialog with keyboard shortcuts
 *          (Enter to accept, Esc to cancel) and high-contrast text fields.
 * @note Blocks input on parent windows while modal; safe for handling unsaved deck attributes.
 */
class SleekInputDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Constructs an input dialog with title, field description, and prefilled text.
     * @param title Header title displayed in the dialog window bar.
     * @param labelText Guidance label placed above the input field.
     * @param initialValue Optional initial text to populate and select.
     * @param parent Optional parent widget.
     */
    explicit SleekInputDialog(
        const QString& title,
        const QString& labelText,
        const QString& initialValue = QString(),
        QWidget* parent = nullptr
    );

    /**
     * @brief Retrieves the trimmed text entered by the user.
     * @return User-entered string from the line edit field.
     */
    [[nodiscard]] QString value() const;

protected:
    /**
     * @brief Intercepts key presses to support Escape cancellation and Return confirmation.
     * @param event Pointer to key event.
     */
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* m_lineEdit = nullptr;
};

/**
 * @brief Minimalist modal dialog for color and opacity customization.
 * @details Displays a curated 24-color preset grid, hex code input field,
 *          and an alpha opacity slider with real-time preview rendering.
 * @note All color computations operate in sRGB 32-bit ARGB space.
 */
class SleekColorDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Constructs a color picker dialog pre-seeded with the current color.
     * @param initialColor Starting color and opacity value.
     * @param parent Optional parent widget.
     */
    explicit SleekColorDialog(const QColor& initialColor, QWidget* parent = nullptr);

    /**
     * @brief Retrieves the final selected QColor including configured alpha channel.
     * @return Resulting QColor chosen by the user.
     */
    [[nodiscard]] QColor selectedColor() const;

private slots:
    /**
     * @brief Updates active color when a preset palette swatch is clicked.
     * @param color Preset color associated with the clicked swatch.
     */
    void onSwatchClicked(const QColor& color);

    /**
     * @brief Parses and validates hex string entered in the text box.
     * @param text Raw hex string (e.g. #RRGGBB).
     */
    void onHexEdited(const QString& text);

    /**
     * @brief Updates the alpha channel of the active color.
     * @param value Opacity percentage between 10 and 100.
     */
    void onOpacityChanged(int value);

    /**
     * @brief Synchronizes the preview swatch and hex display with current state.
     */
    void updatePreview();

private:
    QColor m_color;
    QLineEdit* m_hexEdit = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QLabel* m_opacityLabel = nullptr;
    QWidget* m_previewSwatch = nullptr;
    QLabel* m_previewText = nullptr;
};

/**
 * @brief Minimalist modal dialog for creating a new deck slot.
 * @details Solicits slot name, launch command, color swatch, and insertion
 *          direction (insert to the left or right of current active slot).
 * @note Enforces non-empty name and command requirements before accepting dialog.
 */
class SleekAddDeckDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Constructs the add deck dialog relative to an existing deck name.
     * @param relativeDeckName Label of neighboring slot used in insertion radio buttons.
     * @param parent Optional parent widget.
     */
    explicit SleekAddDeckDialog(
        const QString& relativeDeckName = QString(),
        QWidget* parent = nullptr
    );

    /**
     * @brief Retrieves the trimmed deck name.
     * @return Deck slot name string.
     */
    [[nodiscard]] QString deckName() const;

    /**
     * @brief Retrieves the launch command string.
     * @return Shell command line to execute for this deck.
     */
    [[nodiscard]] QString deckCommand() const;

    /**
     * @brief Retrieves the chosen accent color and opacity.
     * @return Selected QColor for the new deck slot.
     */
    [[nodiscard]] QColor deckColor() const;

    /**
     * @brief Checks whether the user opted to insert left of the active slot.
     * @return True if insert-left is selected, false if insert-right is selected.
     */
    [[nodiscard]] bool insertLeft() const;

protected:
    /**
     * @brief Intercepts key presses to allow Return submission and Escape cancel.
     * @param event Pointer to key event.
     */
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    /**
     * @brief Updates selected color when a palette swatch is clicked.
     * @param color Preset color selected by user.
     */
    void onSwatchClicked(const QColor& color);

    /**
     * @brief Refreshes preview widget color and opacity.
     */
    void updatePreview();

private:
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_commandEdit = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QLabel* m_opacityLabel = nullptr;
    QWidget* m_previewSwatch = nullptr;
    QRadioButton* m_leftRadio = nullptr;
    QRadioButton* m_rightRadio = nullptr;
    QColor m_selectedColor;
};

/**
 * @brief Minimalist modal dialog for reordering deck tabs or profiles.
 * @details Displays items in a vertical list widget with dedicated Move Up
 *          and Move Down buttons. Avoids complex and fragile drag-and-drop gesture bugs.
 * @note Operates on either integer index permutations or string ID orders.
 */
class SleekReorderDialog : public QDialog {
    Q_OBJECT
public:
    /**
     * @brief Constructs reorder dialog for deck slots.
     * @param decks Vector of DeckItemInfo describing the slots to reorder.
     * @param activeIndex Index of the currently active deck to pre-select.
     * @param parent Optional parent widget.
     */
    explicit SleekReorderDialog(
        const QVector<DeckItemInfo>& decks,
        int activeIndex = -1,
        QWidget* parent = nullptr
    );

    /**
     * @brief Constructs reorder dialog for generic items (e.g. profiles).
     * @param title Window title text.
     * @param subtitle Informational subtitle text shown above the list.
     * @param items Vector of ReorderableItem elements.
     * @param activeIndex Index of currently active item to pre-select.
     * @param parent Optional parent widget.
     */
    explicit SleekReorderDialog(
        const QString& title,
        const QString& subtitle,
        const QVector<ReorderableItem>& items,
        int activeIndex = -1,
        QWidget* parent = nullptr
    );

    /**
     * @brief Retrieves the resulting index permutation order.
     * @return Vector of original indices in their new relative order.
     */
    [[nodiscard]] QVector<int> newOrder() const;

    /**
     * @brief Retrieves the resulting string ID order.
     * @return Vector of item IDs in their new relative order.
     */
    [[nodiscard]] QVector<QString> newIdOrder() const;

protected:
    /**
     * @brief Intercepts key presses for Escape cancellation and Enter submission.
     * @param event Pointer to key event.
     */
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    /**
     * @brief Moves the selected item one position upward in the list.
     */
    void onMoveUp();

    /**
     * @brief Moves the selected item one position downward in the list.
     */
    void onMoveDown();

    /**
     * @brief Updates enabled states of Up/Down buttons based on selection row.
     */
    void onSelectionChanged();

private:
    /**
     * @brief Internal UI initialization shared across both constructors.
     * @param title Dialog window title.
     * @param subtitle Header description label.
     * @param items Items to populate in the list widget.
     * @param activeIndex Row index to focus initially.
     */
    void initUI(
        const QString& title,
        const QString& subtitle,
        const QVector<ReorderableItem>& items,
        int activeIndex
    );

    QListWidget* m_listWidget = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;

    /**
     * @brief Refreshes Move Up and Move Down button enabled states.
     */
    void updateButtonStates();
};
