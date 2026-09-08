#include "ProfilePickerWindow.h"
#include "../infrastructure/ConfigManager.h"
#include "SleekDialogs.h"
#include "AppIcon.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QPainter>
#include <QMouseEvent>
#include <QMessageBox>
#include <QMenu>
#include <QStyle>
#include <QKeyEvent>

namespace {
    class ProfileCard : public QFrame {
        Q_OBJECT
    public:
        ProfileCard(const ProfileInfo& info, QWidget* parent = nullptr)
            : QFrame(parent), m_info(info) {
            setFixedSize(140, 180);
            setCursor(Qt::PointingHandCursor);
            setStyleSheet(
                "ProfileCard {"
                "  background-color: #18181b;"
                "  border: 1px solid #27272a;"
                "  border-radius: 8px;"
                "}"
                "ProfileCard:hover {"
                "  border: 1px solid #6366f1;"
                "  background-color: #27272a;"
                "}"
            );

            auto* layout = new QVBoxLayout(this);
            layout->setAlignment(Qt::AlignCenter);
            layout->setSpacing(12);

            // Avatar (initials)
            auto* avatarLabel = new QLabel(this);
            avatarLabel->setFixedSize(64, 64);
            
            QPixmap avatar(64, 64);
            avatar.fill(Qt::transparent);
            QPainter p(&avatar);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(info.accentColor);
            p.setPen(Qt::NoPen);
            p.drawEllipse(0, 0, 64, 64);
            
            p.setPen(Qt::white);
            QFont font = p.font();
            font.setPointSize(24);
            font.setBold(true);
            p.setFont(font);
            QString initial = info.displayName.isEmpty() ? "?" : info.displayName.left(1).toUpper();
            p.drawText(avatar.rect(), Qt::AlignCenter, initial);
            p.end();
            
            avatarLabel->setPixmap(avatar);
            layout->addWidget(avatarLabel, 0, Qt::AlignCenter);

            // Name
            auto* nameLabel = new QLabel(info.displayName, this);
            nameLabel->setStyleSheet("color: white; font-weight: bold; font-size: 14px;");
            nameLabel->setAlignment(Qt::AlignCenter);
            layout->addWidget(nameLabel, 0, Qt::AlignCenter);

            // Deck color bar preview
            ConfigManager profileConfig(info.id);
            (void)profileConfig.loadConfig();
            const auto& decks = profileConfig.getDecks();
            
            auto* colorBar = new QWidget(this);
            colorBar->setFixedHeight(6);
            auto* colorLayout = new QHBoxLayout(colorBar);
            colorLayout->setContentsMargins(0, 0, 0, 0);
            colorLayout->setSpacing(2);
            for (const auto& deck : decks) {
                auto* strip = new QWidget(colorBar);
                strip->setStyleSheet(QString("background-color: %1; border-radius: 3px;").arg(deck.color.name(QColor::HexArgb)));
                colorLayout->addWidget(strip);
            }
            layout->addWidget(colorBar);
        }

        const ProfileInfo& info() const { return m_info; }

    protected:
        void mouseReleaseEvent(QMouseEvent* event) override {
            if (event->button() == Qt::LeftButton) {
                emit clicked(m_info.id);
            } else if (event->button() == Qt::RightButton) {
                emit rightClicked(m_info.id, event->globalPosition().toPoint());
            }
        }
        
    signals:
        void clicked(const QString& id);
        void rightClicked(const QString& id, const QPoint& pos);
        
    private:
        ProfileInfo m_info;
    };
} // namespace

ProfilePickerWindow::ProfilePickerWindow(QWidget* parent)
    : QWidget(parent) {
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(700, 500);

    buildUI();
    rebuildProfileCards();
}

void ProfilePickerWindow::buildUI() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* backgroundFrame = new QFrame(this);
    backgroundFrame->setStyleSheet(
        "QFrame#bgFrame {"
        "  background-color: #09090b;"
        "  border: 1px solid #27272a;"
        "  border-radius: 12px;"
        "}"
    );
    backgroundFrame->setObjectName("bgFrame");
    rootLayout->addWidget(backgroundFrame);

    auto* mainLayout = new QVBoxLayout(backgroundFrame);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    mainLayout->setSpacing(20);

    // Title / Header
    auto* headerLayout = new QHBoxLayout();
    
    auto* logoLabel = new QLabel(this);
    logoLabel->setPixmap(AppIcon::createAppIcon().pixmap(48, 48));
    headerLayout->addWidget(logoLabel);
    
    auto* titleLabel = new QLabel("Which decks?", this);
    titleLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    mainLayout->addLayout(headerLayout);
    mainLayout->addSpacing(10);

    // Absolute position close button relative to backgroundFrame
    auto* closeBtn = new QPushButton("✕", backgroundFrame);
    closeBtn->setFixedSize(32, 32);
    closeBtn->move(700 - 32 - 12, 12); // Top right corner
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; color: #a1a1aa; font-size: 16px; border: none; font-weight: bold; }"
        "QPushButton:hover { color: white; background: #ef4444; border-radius: 16px; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    // Cards Grid
    m_cardsContainer = new QWidget(this);
    m_cardsLayout = new QGridLayout(m_cardsContainer);
    m_cardsLayout->setSpacing(20);
    m_cardsLayout->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_cardsContainer, 1);

    // Bottom Bar
    auto* bottomLayout = new QHBoxLayout();
    m_autoLaunchCheck = new QCheckBox("Always use selected profile on startup", this);
    m_autoLaunchCheck->setStyleSheet(
        "QCheckBox { color: #a1a1aa; font-size: 13px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border-radius: 4px; border: 1px solid #3f3f46; background: #18181b; }"
        "QCheckBox::indicator:checked { image: url(data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='%236366f1' stroke-width='4' stroke-linecap='round' stroke-linejoin='round'><path d='M18 6 6 18'/><path d='m6 6 12 12'/></svg>); }"
    );
    bottomLayout->addWidget(m_autoLaunchCheck);
    bottomLayout->addStretch();
    
    auto* addProfileBtn = new QPushButton("+ Add Profile", this);
    addProfileBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: transparent;"
        "  color: #6366f1;"
        "  border: 1px solid #6366f1;"
        "  border-radius: 6px;"
        "  padding: 8px 16px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(99, 102, 241, 0.1);"
        "}"
    );
    connect(addProfileBtn, &QPushButton::clicked, this, &ProfilePickerWindow::onAddProfileClicked);
    bottomLayout->addWidget(addProfileBtn);

    mainLayout->addLayout(bottomLayout);
}

void ProfilePickerWindow::rebuildProfileCards() {
    // Clear layout
    QLayoutItem* item;
    while ((item = m_cardsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    auto profiles = ConfigManager::listProfiles();
    int row = 0;
    int col = 0;
    int maxCols = 3;

    for (const auto& p : profiles) {
        QWidget* card = createProfileCard(p);
        m_cardsLayout->addWidget(card, row, col);
        
        col++;
        if (col >= maxCols) {
            col = 0;
            row++;
        }
    }
}

QWidget* ProfilePickerWindow::createProfileCard(const ProfileInfo& profile) {
    auto* card = new ProfileCard(profile, this);
    
    connect(card, &ProfileCard::clicked, this, &ProfilePickerWindow::onProfileCardClicked);
    connect(card, &ProfileCard::rightClicked, this, [this](const QString& id, const QPoint& pos) {
        SleekContextMenu menu(this);
        
        auto* renameAction = menu.addAction(getSleekMenuIcon(SleekMenuIcon::EditName), "Rename Profile...");
        auto* colorAction = menu.addAction(getSleekMenuIcon(SleekMenuIcon::ChangeColor), "Change Color...");
        menu.addSeparator();
        auto* deleteAction = menu.addAction(getSleekMenuIcon(SleekMenuIcon::DeleteDeck), "Delete Profile");
        
        // Prevent deleting the only profile
        if (ConfigManager::listProfiles().size() <= 1) {
            deleteAction->setEnabled(false);
        }

        QAction* selected = menu.exec(pos);
        if (selected == renameAction) {
            onEditProfileName(id);
        } else if (selected == colorAction) {
            onEditProfileColor(id);
        } else if (selected == deleteAction) {
            onDeleteProfile(id);
        }
    });

    return card;
}

void ProfilePickerWindow::onProfileCardClicked(const QString& profileId) {
    m_selected = true;
    if (m_autoLaunchCheck->isChecked()) {
        ConfigManager::setAutoLaunchProfile(profileId);
    }
    emit profileSelected(profileId);
}

void ProfilePickerWindow::onAddProfileClicked() {
    SleekInputDialog nameDialog("Add Profile", "Profile Name:", "", this);
    nameDialog.adjustSize();
    nameDialog.move(geometry().center() - nameDialog.rect().center());
    if (nameDialog.exec() == QDialog::Accepted) {
        QString name = nameDialog.value();
        if (!name.isEmpty()) {
            SleekColorDialog colorDialog(QColor("#802563eb"), this);
            colorDialog.adjustSize();
            colorDialog.move(geometry().center() - colorDialog.rect().center());
            if (colorDialog.exec() == QDialog::Accepted) {
                ConfigManager::createProfile(name, colorDialog.selectedColor());
                rebuildProfileCards();
            }
        }
    }
}

void ProfilePickerWindow::onDeleteProfile(const QString& profileId) {
    // Basic confirmation logic, keep it sleek if we can, but QMessageBox is safe.
    QMessageBox msgBox(this);
    msgBox.setStyleSheet("QMessageBox { background-color: #18181b; color: white; } QLabel { color: white; } QPushButton { background-color: #27272a; color: white; padding: 6px 12px; border: 1px solid #3f3f46; border-radius: 4px; } QPushButton:hover { background-color: #3f3f46; }");
    msgBox.setText(QString("Are you sure you want to delete profile '%1'?\nThis will delete all its decks and settings permanently.").arg(profileId));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);
    if (msgBox.exec() == QMessageBox::Yes) {
        ConfigManager::deleteProfile(profileId);
        rebuildProfileCards();
    }
}

void ProfilePickerWindow::onEditProfileName(const QString& profileId) {
    auto profiles = ConfigManager::listProfiles();
    QString currentName;
    for (const auto& p : profiles) {
        if (p.id == profileId) currentName = p.displayName;
    }
    
    SleekInputDialog dialog("Rename Profile", "New Name:", currentName, this);
    dialog.adjustSize();
    dialog.move(geometry().center() - dialog.rect().center());
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.value();
        if (!name.isEmpty() && name != currentName) {
            ConfigManager::renameProfile(profileId, name);
            rebuildProfileCards();
        }
    }
}

void ProfilePickerWindow::onEditProfileColor(const QString& profileId) {
    auto profiles = ConfigManager::listProfiles();
    QColor currentColor;
    for (const auto& p : profiles) {
        if (p.id == profileId) currentColor = p.accentColor;
    }
    
    SleekColorDialog dialog(currentColor, this);
    dialog.adjustSize();
    dialog.move(geometry().center() - dialog.rect().center());
    if (dialog.exec() == QDialog::Accepted) {
        ConfigManager::updateProfileColor(profileId, dialog.selectedColor());
        rebuildProfileCards();
    }
}

void ProfilePickerWindow::closeEvent(QCloseEvent* event) {
    if (!m_selected) {
        emit closedWithoutSelection();
    }
    QWidget::closeEvent(event);
}

void ProfilePickerWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void ProfilePickerWindow::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void ProfilePickerWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void ProfilePickerWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        move(screenGeometry.center() - rect().center());
    }
}

#include "ProfilePickerWindow.moc"
