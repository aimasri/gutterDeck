#pragma once

#include <QWidget>
#include <QString>
#include <QVector>

class QCheckBox;
class QVBoxLayout;
class QGridLayout;
struct ProfileInfo;

class ProfilePickerWindow : public QWidget {
    Q_OBJECT
public:
    explicit ProfilePickerWindow(QWidget* parent = nullptr);

signals:
    void profileSelected(const QString& profileId);
    void closedWithoutSelection();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private slots:
    void onProfileCardClicked(const QString& profileId);
    void onAddProfileClicked();
    void onDeleteProfile(const QString& profileId);
    void onEditProfileName(const QString& profileId);
    void onEditProfileColor(const QString& profileId);

private:
    void buildUI();
    void rebuildProfileCards();
    QWidget* createProfileCard(const ProfileInfo& profile);
    void applyTheme();

    QWidget* m_cardsContainer = nullptr;
    QGridLayout* m_cardsLayout = nullptr;
    QCheckBox* m_autoLaunchCheck = nullptr;
    bool m_selected = false;
    QPoint m_dragPosition;
};
