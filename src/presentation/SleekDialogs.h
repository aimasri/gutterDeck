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
 */
struct DeckItemInfo {
    int originalIndex = 0;
    QString name;
    QString command;
    QColor color;
};

enum class SleekMenuIcon {
    EditName,
    EditCommand,
    ChangeColor,
    AddDeck,
    ReorderDecks,
    DeleteDeck,
    CloseApp
};

[[nodiscard]] QIcon getSleekMenuIcon(SleekMenuIcon type);

/**
 * @brief Sleek, dark minimalist context menu for gutter tabs.
 */
class SleekContextMenu : public QMenu {
    Q_OBJECT
public:
    explicit SleekContextMenu(QWidget* parent = nullptr);
};

/**
 * @brief Minimalist modal dialog for text editing (Deck Name, Launch Command).
 */
class SleekInputDialog : public QDialog {
    Q_OBJECT
public:
    explicit SleekInputDialog(
        const QString& title,
        const QString& labelText,
        const QString& initialValue = QString(),
        QWidget* parent = nullptr
    );

    [[nodiscard]] QString value() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* m_lineEdit = nullptr;
};

/**
 * @brief Minimalist modal dialog for color and opacity customization.
 */
class SleekColorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SleekColorDialog(const QColor& initialColor, QWidget* parent = nullptr);

    [[nodiscard]] QColor selectedColor() const;

private slots:
    void onSwatchClicked(const QColor& color);
    void onHexEdited(const QString& text);
    void onOpacityChanged(int value);
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
 */
class SleekAddDeckDialog : public QDialog {
    Q_OBJECT
public:
    explicit SleekAddDeckDialog(
        const QString& relativeDeckName = QString(),
        QWidget* parent = nullptr
    );

    [[nodiscard]] QString deckName() const;
    [[nodiscard]] QString deckCommand() const;
    [[nodiscard]] QColor deckColor() const;
    [[nodiscard]] bool insertLeft() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSwatchClicked(const QColor& color);
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
 * @brief Minimalist modal dialog for reordering deck tabs.
 */
class SleekReorderDialog : public QDialog {
    Q_OBJECT
public:
    explicit SleekReorderDialog(
        const QVector<DeckItemInfo>& decks,
        int activeIndex = -1,
        QWidget* parent = nullptr
    );

    [[nodiscard]] QVector<int> newOrder() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onMoveUp();
    void onMoveDown();
    void onSelectionChanged();

private:
    QListWidget* m_listWidget = nullptr;
    QPushButton* m_upBtn = nullptr;
    QPushButton* m_downBtn = nullptr;
    void updateButtonStates();
};
