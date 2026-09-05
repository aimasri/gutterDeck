#include "SleekDialogs.h"

#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace {
const char* const DIALOG_STYLE = R"(
    #dialogCard {
        background-color: #18181b;
        border: 1px solid #3f3f46;
        border-radius: 10px;
    }
    QRadioButton {
        color: #f4f4f5;
        font-size: 12px;
        spacing: 6px;
    }
    QRadioButton::indicator {
        width: 14px;
        height: 14px;
        border-radius: 7px;
        border: 1px solid #52525b;
        background-color: #09090b;
    }
    QRadioButton::indicator:checked {
        background-color: #6366f1;
        border: 2px solid #ffffff;
    }
    QListWidget {
        background-color: #09090b;
        border: 1px solid #27272a;
        border-radius: 6px;
        color: #f4f4f5;
        padding: 4px;
        font-size: 13px;
        outline: none;
    }
    QListWidget::item {
        padding: 8px 10px;
        border-radius: 5px;
        margin: 2px 0px;
    }
    QListWidget::item:selected {
        background-color: #6366f1;
        color: #ffffff;
    }
    QListWidget::item:hover:!selected {
        background-color: #27272a;
    }
    QLabel {
        color: #f4f4f5;
        font-family: sans-serif;
    }
    QLabel#titleLabel {
        font-size: 15px;
        font-weight: bold;
        color: #ffffff;
    }
    QLabel#subLabel {
        font-size: 12px;
        color: #a1a1aa;
    }
    QLineEdit {
        background-color: #09090b;
        border: 1px solid #27272a;
        border-radius: 6px;
        color: #f4f4f5;
        padding: 8px 12px;
        font-size: 13px;
        selection-background-color: #6366f1;
    }
    QLineEdit:focus {
        border: 1px solid #6366f1;
    }
    QSlider::groove:horizontal {
        height: 6px;
        background: #27272a;
        border-radius: 3px;
    }
    QSlider::sub-page:horizontal {
        background: #6366f1;
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        background: #ffffff;
        border: 1px solid #6366f1;
        width: 14px;
        margin-top: -4px;
        margin-bottom: -4px;
        border-radius: 7px;
    }
    QPushButton#cancelBtn {
        background-color: #27272a;
        color: #d4d4d8;
        border: none;
        border-radius: 6px;
        padding: 7px 16px;
        font-size: 12px;
        font-weight: 600;
    }
    QPushButton#cancelBtn:hover {
        background-color: #3f3f46;
        color: #ffffff;
    }
    QPushButton#actionBtn {
        background-color: #6366f1;
        color: #ffffff;
        border: none;
        border-radius: 6px;
        padding: 7px 20px;
        font-size: 12px;
        font-weight: 600;
    }
    QPushButton#actionBtn:hover {
        background-color: #4f46e5;
    }
)";

const char* const MENU_STYLE = R"(
    QMenu {
        background-color: #18181b;
        border: 1px solid #3f3f46;
        border-radius: 8px;
        padding: 6px;
    }
    QMenu::item {
        background-color: transparent;
        color: #f4f4f5;
        padding: 7px 18px 7px 10px;
        margin: 2px 0px;
        border-radius: 5px;
        font-family: sans-serif;
        font-size: 13px;
        font-weight: 500;
    }
    QMenu::item:selected {
        background-color: #27272a;
        color: #ffffff;
    }
    QMenu::item:disabled {
        color: #71717a;
    }
    QMenu::icon {
        padding-left: 8px;
    }
    QMenu::separator {
        height: 1px;
        background-color: #27272a;
        margin: 5px 6px;
    }
)";
} // namespace

// ==========================================
// SleekContextMenu & Vector Icons
// ==========================================
QIcon getSleekMenuIcon(SleekMenuIcon type) {
    QPixmap pix(32, 32);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    switch (type) {
        case SleekMenuIcon::EditName: {
            QPen pen(QColor("#a1a1aa"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            QPainterPath path;
            path.moveTo(8, 24);
            path.lineTo(21, 11);
            path.lineTo(24, 14);
            path.lineTo(11, 27);
            path.closeSubpath();
            p.strokePath(path, pen);
            p.drawLine(QPointF(8, 24), QPointF(11, 27));
            p.drawLine(QPointF(6, 27), QPointF(8, 24));
            p.drawLine(QPointF(6, 27), QPointF(11, 27));
            break;
        }
        case SleekMenuIcon::EditCommand: {
            QPen pen(QColor("#a1a1aa"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(QPointF(7, 10), QPointF(14, 16));
            p.drawLine(QPointF(14, 16), QPointF(7, 22));
            p.drawLine(QPointF(16, 22), QPointF(25, 22));
            break;
        }
        case SleekMenuIcon::ChangeColor: {
            QPen pen(QColor("#a1a1aa"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawEllipse(QRectF(6, 6, 20, 20));
            p.setPen(Qt::NoPen);
            p.setBrush(QBrush(QColor("#ef4444")));
            p.drawEllipse(QRectF(10, 11, 4, 4));
            p.setBrush(QBrush(QColor("#10b981")));
            p.drawEllipse(QRectF(17, 9, 4, 4));
            p.setBrush(QBrush(QColor("#6366f1")));
            p.drawEllipse(QRectF(20, 16, 4, 4));
            break;
        }
        case SleekMenuIcon::AddDeck: {
            QPen pen(QColor("#10b981"), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(QPointF(16, 7), QPointF(16, 25));
            p.drawLine(QPointF(7, 16), QPointF(25, 16));
            break;
        }
        case SleekMenuIcon::ReorderDecks: {
            QPen pen(QColor("#a1a1aa"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(QPointF(10, 24), QPointF(10, 8));
            p.drawLine(QPointF(6, 12), QPointF(10, 8));
            p.drawLine(QPointF(14, 12), QPointF(10, 8));
            p.drawLine(QPointF(22, 8), QPointF(22, 24));
            p.drawLine(QPointF(18, 20), QPointF(22, 24));
            p.drawLine(QPointF(26, 20), QPointF(22, 24));
            break;
        }
        case SleekMenuIcon::DeleteDeck: {
            QPen pen(QColor("#f87171"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            // Lid
            p.drawLine(QPointF(7, 10), QPointF(25, 10));
            p.drawLine(QPointF(12, 10), QPointF(12, 7));
            p.drawLine(QPointF(12, 7), QPointF(20, 7));
            p.drawLine(QPointF(20, 7), QPointF(20, 10));
            // Can body
            QPainterPath path;
            path.moveTo(9, 10);
            path.lineTo(10, 25);
            path.lineTo(22, 25);
            path.lineTo(23, 10);
            p.strokePath(path, pen);
            // Ribs
            p.drawLine(QPointF(13, 14), QPointF(13, 21));
            p.drawLine(QPointF(19, 14), QPointF(19, 21));
            break;
        }
        case SleekMenuIcon::CloseApp: {
            QPen pen(QColor("#ef4444"), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.drawLine(QPointF(8, 25), QPointF(8, 7));
            p.drawLine(QPointF(8, 7), QPointF(19, 7));
            p.drawLine(QPointF(19, 7), QPointF(19, 10));
            p.drawLine(QPointF(19, 22), QPointF(19, 25));
            p.drawLine(QPointF(19, 25), QPointF(8, 25));
            p.drawLine(QPointF(13, 16), QPointF(25, 16));
            p.drawLine(QPointF(21, 12), QPointF(25, 16));
            p.drawLine(QPointF(21, 20), QPointF(25, 16));
            break;
        }
    }
    p.end();

    QIcon icon;
    icon.addPixmap(pix, QIcon::Normal);

    QPixmap disabledPix = pix;
    {
        QPainter dp(&disabledPix);
        dp.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        dp.fillRect(disabledPix.rect(), QColor(0, 0, 0, 70));
        dp.end();
    }
    icon.addPixmap(disabledPix, QIcon::Disabled);

    return icon;
}

SleekContextMenu::SleekContextMenu(QWidget* parent)
    : QMenu(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setStyleSheet(MENU_STYLE);
}

// ==========================================
// SleekInputDialog
// ==========================================
SleekInputDialog::SleekInputDialog(
    const QString& title,
    const QString& labelText,
    const QString& initialValue,
    QWidget* parent
) : QDialog(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(380);
    setStyleSheet(DIALOG_STYLE);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(this);
    card->setObjectName("dialogCard");
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(14);

    auto* titleLbl = new QLabel(title, card);
    titleLbl->setObjectName("titleLabel");
    cardLayout->addWidget(titleLbl);

    if (!labelText.isEmpty()) {
        auto* subLbl = new QLabel(labelText, card);
        subLbl->setObjectName("subLabel");
        cardLayout->addWidget(subLbl);
    }

    m_lineEdit = new QLineEdit(initialValue, card);
    m_lineEdit->selectAll();
    cardLayout->addWidget(m_lineEdit);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    auto* cancelBtn = new QPushButton("Cancel", card);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton("Save", card);
    saveBtn->setObjectName("actionBtn");
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(saveBtn);

    cardLayout->addLayout(btnLayout);

    m_lineEdit->setFocus();
}

QString SleekInputDialog::value() const {
    return m_lineEdit ? m_lineEdit->text().trimmed() : QString();
}

void SleekInputDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}

// ==========================================
// SleekColorDialog
// ==========================================
SleekColorDialog::SleekColorDialog(const QColor& initialColor, QWidget* parent)
    : QDialog(parent),
      m_color(initialColor.isValid() ? initialColor : QColor("#806366F1")) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(340);
    setStyleSheet(DIALOG_STYLE);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(this);
    card->setObjectName("dialogCard");
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(14);

    auto* titleLbl = new QLabel("Color & Opacity", card);
    titleLbl->setObjectName("titleLabel");
    cardLayout->addWidget(titleLbl);

    // 1. Palette Swatch Grid
    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(8);

    const QVector<QString> palette = {
        "#ef4444", "#f97316", "#f59e0b", "#10b981",
        "#06b6d4", "#3b82f6", "#6366f1", "#8b5cf6",
        "#ec4899", "#64748b", "#22c55e", "#14b8a6",
        "#a855f7", "#e11d48", "#334155", "#27272a"
    };

    for (int i = 0; i < palette.size(); ++i) {
        QColor c(palette[i]);
        auto* swatchBtn = new QPushButton(card);
        swatchBtn->setFixedSize(28, 28);
        swatchBtn->setStyleSheet(QString(
            "QPushButton {"
            "    background-color: %1;"
            "    border: 1px solid rgba(255, 255, 255, 0.2);"
            "    border-radius: 5px;"
            "}"
            "QPushButton:hover {"
            "    border: 2px solid #ffffff;"
            "}"
        ).arg(palette[i]));
        connect(swatchBtn, &QPushButton::clicked, this, [this, c]() {
            onSwatchClicked(c);
        });
        gridLayout->addWidget(swatchBtn, i / 4, i % 4);
    }
    cardLayout->addLayout(gridLayout);

    // 2. Hex Input
    auto* hexLayout = new QHBoxLayout();
    auto* hexLabel = new QLabel("Hex:", card);
    hexLabel->setObjectName("subLabel");
    hexLayout->addWidget(hexLabel);

    m_hexEdit = new QLineEdit(m_color.name(QColor::HexRgb).toUpper(), card);
    QRegularExpression hexRegex("^#?[0-9A-Fa-f]{6}$");
    m_hexEdit->setValidator(new QRegularExpressionValidator(hexRegex, m_hexEdit));
    connect(m_hexEdit, &QLineEdit::textChanged, this, &SleekColorDialog::onHexEdited);
    hexLayout->addWidget(m_hexEdit);
    cardLayout->addLayout(hexLayout);

    // 3. Opacity Slider
    int initialAlphaPercent = static_cast<int>((m_color.alphaF() * 100.0) + 0.5);
    if (initialAlphaPercent <= 0) {
        initialAlphaPercent = 50;
    }

    auto* opacityHeader = new QHBoxLayout();
    auto* opacityTitle = new QLabel("Opacity:", card);
    opacityTitle->setObjectName("subLabel");
    opacityHeader->addWidget(opacityTitle);

    m_opacityLabel = new QLabel(QString("%1%").arg(initialAlphaPercent), card);
    m_opacityLabel->setObjectName("subLabel");
    opacityHeader->addStretch(1);
    opacityHeader->addWidget(m_opacityLabel);
    cardLayout->addLayout(opacityHeader);

    m_opacitySlider = new QSlider(Qt::Horizontal, card);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(initialAlphaPercent);
    connect(m_opacitySlider, &QSlider::valueChanged, this, &SleekColorDialog::onOpacityChanged);
    cardLayout->addWidget(m_opacitySlider);

    // 4. Live Preview Box
    m_previewSwatch = new QWidget(card);
    m_previewSwatch->setFixedHeight(40);
    m_previewSwatch->setStyleSheet("border-radius: 6px;");

    auto* previewInner = new QHBoxLayout(m_previewSwatch);
    m_previewText = new QLabel("Preview Tab", m_previewSwatch);
    m_previewText->setStyleSheet("color: white; font-weight: bold; font-size: 12px;");
    previewInner->addWidget(m_previewText, 0, Qt::AlignCenter);
    cardLayout->addWidget(m_previewSwatch);

    updatePreview();

    // 5. Actions
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    auto* cancelBtn = new QPushButton("Cancel", card);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* applyBtn = new QPushButton("Apply", card);
    applyBtn->setObjectName("actionBtn");
    connect(applyBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(applyBtn);

    cardLayout->addLayout(btnLayout);
}

void SleekColorDialog::onSwatchClicked(const QColor& color) {
    int alpha = m_color.alpha();
    m_color = color;
    m_color.setAlpha(alpha);

    if (m_hexEdit) {
        m_hexEdit->setText(m_color.name(QColor::HexRgb).toUpper());
    }
    updatePreview();
}

void SleekColorDialog::onHexEdited(const QString& text) {
    QString hex = text.trimmed();
    if (!hex.startsWith('#')) {
        hex.prepend('#');
    }
    if (QColor::isValidColorName(hex)) {
        int alpha = m_color.alpha();
        m_color = QColor(hex);
        m_color.setAlpha(alpha);
        updatePreview();
    }
}

void SleekColorDialog::onOpacityChanged(int value) {
    m_color.setAlphaF(value / 100.0);
    if (m_opacityLabel) {
        m_opacityLabel->setText(QString("%1%").arg(value));
    }
    updatePreview();
}

void SleekColorDialog::updatePreview() {
    if (m_previewSwatch) {
        m_previewSwatch->setStyleSheet(QString(
            "background-color: %1; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 6px;"
        ).arg(m_color.name(QColor::HexArgb)));
    }
}

QColor SleekColorDialog::selectedColor() const {
    return m_color;
}

// ==========================================
// SleekAddDeckDialog
// ==========================================
SleekAddDeckDialog::SleekAddDeckDialog(const QString& relativeDeckName, QWidget* parent)
    : QDialog(parent),
      m_selectedColor(QColor("#806366F1")) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(380);
    setStyleSheet(DIALOG_STYLE);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(this);
    card->setObjectName("dialogCard");
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(12);

    auto* titleLbl = new QLabel("Add New Deck", card);
    titleLbl->setObjectName("titleLabel");
    cardLayout->addWidget(titleLbl);

    // Deck Name
    auto* nameLbl = new QLabel("Deck Label:", card);
    nameLbl->setObjectName("subLabel");
    cardLayout->addWidget(nameLbl);

    m_nameEdit = new QLineEdit(card);
    m_nameEdit->setPlaceholderText("New Deck");
    connect(m_nameEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
    cardLayout->addWidget(m_nameEdit);

    // Launch Command
    auto* cmdLbl = new QLabel("Launch Command:", card);
    cmdLbl->setObjectName("subLabel");
    cardLayout->addWidget(cmdLbl);

    m_commandEdit = new QLineEdit(card);
    m_commandEdit->setPlaceholderText("x-terminal-emulator");
    connect(m_commandEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
    cardLayout->addWidget(m_commandEdit);

    // Placement Option (Left vs Right)
    auto* placeLbl = new QLabel("Placement:", card);
    placeLbl->setObjectName("subLabel");
    cardLayout->addWidget(placeLbl);

    auto* radioLayout = new QHBoxLayout();
    radioLayout->setSpacing(16);

    QString leftText = relativeDeckName.isEmpty()
        ? QStringLiteral("Left side")
        : QStringLiteral("Left of \"%1\"").arg(relativeDeckName);
    QString rightText = relativeDeckName.isEmpty()
        ? QStringLiteral("Right side")
        : QStringLiteral("Right of \"%1\"").arg(relativeDeckName);

    m_leftRadio = new QRadioButton(leftText, card);
    m_rightRadio = new QRadioButton(rightText, card);
    m_rightRadio->setChecked(true);

    radioLayout->addWidget(m_leftRadio);
    radioLayout->addWidget(m_rightRadio);
    radioLayout->addStretch(1);
    cardLayout->addLayout(radioLayout);

    // Color Swatches
    auto* colorLbl = new QLabel("Deck Tint:", card);
    colorLbl->setObjectName("subLabel");
    cardLayout->addWidget(colorLbl);

    auto* gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);

    const QVector<QString> palette = {
        "#ef4444", "#f97316", "#f59e0b", "#10b981",
        "#06b6d4", "#3b82f6", "#6366f1", "#8b5cf6"
    };

    for (int i = 0; i < palette.size(); ++i) {
        QColor c(palette[i]);
        auto* swatchBtn = new QPushButton(card);
        swatchBtn->setFixedSize(26, 26);
        swatchBtn->setStyleSheet(QString(
            "QPushButton { background-color: %1; border: 1px solid rgba(255,255,255,0.2); border-radius: 4px; }"
            "QPushButton:hover { border: 2px solid #ffffff; }"
        ).arg(palette[i]));
        connect(swatchBtn, &QPushButton::clicked, this, [this, c]() {
            onSwatchClicked(c);
        });
        gridLayout->addWidget(swatchBtn, i / 4, i % 4);
    }
    cardLayout->addLayout(gridLayout);

    // Opacity
    auto* opacityHeader = new QHBoxLayout();
    auto* opTitle = new QLabel("Opacity:", card);
    opTitle->setObjectName("subLabel");
    opacityHeader->addWidget(opTitle);

    m_opacityLabel = new QLabel("50%", card);
    m_opacityLabel->setObjectName("subLabel");
    opacityHeader->addStretch(1);
    opacityHeader->addWidget(m_opacityLabel);
    cardLayout->addLayout(opacityHeader);

    m_opacitySlider = new QSlider(Qt::Horizontal, card);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(50);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int val) {
        m_selectedColor.setAlphaF(val / 100.0);
        if (m_opacityLabel) {
            m_opacityLabel->setText(QString("%1%").arg(val));
        }
        updatePreview();
    });
    cardLayout->addWidget(m_opacitySlider);

    // Preview
    m_previewSwatch = new QWidget(card);
    m_previewSwatch->setFixedHeight(36);
    cardLayout->addWidget(m_previewSwatch);
    updatePreview();

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    auto* cancelBtn = new QPushButton("Cancel", card);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* addBtn = new QPushButton("Add Deck", card);
    addBtn->setObjectName("actionBtn");
    addBtn->setDefault(true);
    connect(addBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(addBtn);

    cardLayout->addLayout(btnLayout);

    m_nameEdit->setFocus();
}

void SleekAddDeckDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}

void SleekAddDeckDialog::onSwatchClicked(const QColor& color) {
    int alpha = m_selectedColor.alpha();
    m_selectedColor = color;
    m_selectedColor.setAlpha(alpha);
    updatePreview();
}

void SleekAddDeckDialog::updatePreview() {
    if (m_previewSwatch) {
        m_previewSwatch->setStyleSheet(QString(
            "background-color: %1; border: 1px solid rgba(255, 255, 255, 0.2); border-radius: 6px;"
        ).arg(m_selectedColor.name(QColor::HexArgb)));
    }
}

QString SleekAddDeckDialog::deckName() const {
    QString name = m_nameEdit ? m_nameEdit->text().trimmed() : QString();
    if (name.isEmpty() && m_nameEdit && !m_nameEdit->placeholderText().isEmpty()) {
        return m_nameEdit->placeholderText();
    }
    return name;
}

QString SleekAddDeckDialog::deckCommand() const {
    QString cmd = m_commandEdit ? m_commandEdit->text().trimmed() : QString();
    if (cmd.isEmpty() && m_commandEdit && !m_commandEdit->placeholderText().isEmpty()) {
        return m_commandEdit->placeholderText();
    }
    return cmd;
}

bool SleekAddDeckDialog::insertLeft() const {
    return m_leftRadio && m_leftRadio->isChecked();
}

QColor SleekAddDeckDialog::deckColor() const {
    return m_selectedColor;
}

// ==========================================
// SleekReorderDialog
// ==========================================
SleekReorderDialog::SleekReorderDialog(
    const QVector<DeckItemInfo>& decks,
    int activeIndex,
    QWidget* parent
) : QDialog(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedWidth(420);
    setStyleSheet(DIALOG_STYLE);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* card = new QFrame(this);
    card->setObjectName("dialogCard");
    rootLayout->addWidget(card);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(20, 20, 20, 20);
    cardLayout->setSpacing(12);

    auto* titleLbl = new QLabel("Reorder Decks", card);
    titleLbl->setObjectName("titleLabel");
    cardLayout->addWidget(titleLbl);

    auto* subLbl = new QLabel("Select a deck tab and use Move Up / Move Down to change order:", card);
    subLbl->setObjectName("subLabel");
    subLbl->setWordWrap(true);
    cardLayout->addWidget(subLbl);

    auto* listAndBtnLayout = new QHBoxLayout();
    listAndBtnLayout->setSpacing(10);

    m_listWidget = new QListWidget(card);
    m_listWidget->setFixedHeight(200);

    for (int i = 0; i < decks.size(); ++i) {
        auto* item = new QListWidgetItem(m_listWidget);
        item->setText(decks[i].name);
        item->setData(Qt::UserRole, decks[i].originalIndex);

        // Color badge
        QPixmap pix(12, 12);
        pix.fill(decks[i].color.isValid() ? decks[i].color : QColor("#6366f1"));
        item->setIcon(QIcon(pix));
    }

    if (activeIndex >= 0 && activeIndex < m_listWidget->count()) {
        m_listWidget->setCurrentRow(activeIndex);
    } else if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
    }

    listAndBtnLayout->addWidget(m_listWidget, 1);

    auto* orderBtnLayout = new QVBoxLayout();
    orderBtnLayout->setSpacing(8);
    orderBtnLayout->addStretch(1);

    m_upBtn = new QPushButton("▲ Up", card);
    m_upBtn->setObjectName("cancelBtn");
    connect(m_upBtn, &QPushButton::clicked, this, &SleekReorderDialog::onMoveUp);
    orderBtnLayout->addWidget(m_upBtn);

    m_downBtn = new QPushButton("▼ Down", card);
    m_downBtn->setObjectName("cancelBtn");
    connect(m_downBtn, &QPushButton::clicked, this, &SleekReorderDialog::onMoveDown);
    orderBtnLayout->addWidget(m_downBtn);

    orderBtnLayout->addStretch(1);
    listAndBtnLayout->addLayout(orderBtnLayout);

    cardLayout->addLayout(listAndBtnLayout);

    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &SleekReorderDialog::onSelectionChanged);
    updateButtonStates();

    // Bottom action buttons
    auto* btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(10);
    btnLayout->addStretch(1);

    auto* cancelBtn = new QPushButton("Cancel", card);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton("Save Order", card);
    saveBtn->setObjectName("actionBtn");
    connect(saveBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(saveBtn);

    cardLayout->addLayout(btnLayout);
}

void SleekReorderDialog::onMoveUp() {
    int row = m_listWidget->currentRow();
    if (row > 0) {
        auto* item = m_listWidget->takeItem(row);
        m_listWidget->insertItem(row - 1, item);
        m_listWidget->setCurrentRow(row - 1);
        updateButtonStates();
    }
}

void SleekReorderDialog::onMoveDown() {
    int row = m_listWidget->currentRow();
    if (row >= 0 && row < m_listWidget->count() - 1) {
        auto* item = m_listWidget->takeItem(row);
        m_listWidget->insertItem(row + 1, item);
        m_listWidget->setCurrentRow(row + 1);
        updateButtonStates();
    }
}

void SleekReorderDialog::onSelectionChanged() {
    updateButtonStates();
}

void SleekReorderDialog::updateButtonStates() {
    int row = m_listWidget ? m_listWidget->currentRow() : -1;
    int count = m_listWidget ? m_listWidget->count() : 0;
    if (m_upBtn) {
        m_upBtn->setEnabled(row > 0);
    }
    if (m_downBtn) {
        m_downBtn->setEnabled(row >= 0 && row < count - 1);
    }
}

QVector<int> SleekReorderDialog::newOrder() const {
    QVector<int> order;
    if (!m_listWidget) {
        return order;
    }
    order.reserve(m_listWidget->count());
    for (int i = 0; i < m_listWidget->count(); ++i) {
        order.append(m_listWidget->item(i)->data(Qt::UserRole).toInt());
    }
    return order;
}

void SleekReorderDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        reject();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}
