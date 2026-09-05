#include "AppLauncher.h"
#include "../infrastructure/ConfigManager.h"

#include <QDebug>

AppLauncher::AppLauncher(const ConfigManager& config, QObject* parent)
    : QObject(parent),
      m_config(config) {
    m_staggerTimer.setSingleShot(true);
    connect(&m_staggerTimer, &QTimer::timeout, this, &AppLauncher::onLaunchNextStaggered);
}

void AppLauncher::launchAll() {
    const auto& decks = m_config.getDecks();
    m_pids.clear();
    m_pids.resize(decks.size(), 0);
    m_currentIndex = 0;

    if (!decks.isEmpty()) {
        onLaunchNextStaggered();
    } else {
        emit allDecksLaunched();
    }
}

qint64 AppLauncher::launchDeck(int index) {
    const auto& decks = m_config.getDecks();
    if (index < 0 || index >= decks.size()) {
        qWarning() << "Cannot launch deck with out-of-bounds index:" << index;
        return -1;
    }

    const DeckConfig& deck = decks.at(index);
    if (deck.command.trimmed().isEmpty()) {
        qWarning() << "Deck command is empty for deck:" << deck.name;
        return -1;
    }

    qint64 pid = 0;
    QString cmd = deck.command.trimmed();
    if (!cmd.contains(';') && !cmd.contains('&') && !cmd.contains('|') && !cmd.startsWith("exec ")) {
        cmd = "exec " + cmd;
    }

    bool started = QProcess::startDetached("/bin/sh", QStringList() << "-c" << cmd,
                                          QString(), &pid);
    if (!started || pid <= 0) {
        qCritical() << "Failed to start command for deck:" << deck.name
                    << "Command:" << deck.command;
        return -1;
    }

    qDebug() << "Successfully launched deck [" << deck.name << "] with PID:" << pid;

    if (index < m_pids.size()) {
        m_pids[index] = pid;
    } else {
        m_pids.resize(index + 1, 0);
        m_pids[index] = pid;
    }

    emit deckLaunched(index, pid, deck.command);
    return pid;
}

void AppLauncher::onLaunchNextStaggered() {
    const auto& decks = m_config.getDecks();
    if (m_currentIndex < decks.size()) {
        launchDeck(m_currentIndex);
        ++m_currentIndex;
        // Stagger next launch by 250ms to prevent X11 server configure request storms
        m_staggerTimer.start(250);
    } else {
        qDebug() << "All deck applications have been dispatched.";
        emit allDecksLaunched();
    }
}

qint64 AppLauncher::getDeckPid(int index) const {
    if (index >= 0 && index < m_pids.size()) {
        return m_pids.at(index);
    }
    return 0;
}

int AppLauncher::findDeckIndexByPid(qint64 pid) const {
    if (pid <= 0) {
        return -1;
    }
    for (int i = 0; i < m_pids.size(); ++i) {
        if (m_pids.at(i) == pid) {
            return i;
        }
    }
    return -1;
}

void AppLauncher::insertDeck(int index) {
    int clamped = std::max(0, std::min(index, static_cast<int>(m_pids.size())));
    m_pids.insert(clamped, 0);
}

void AppLauncher::removeDeck(int index) {
    if (index >= 0 && index < m_pids.size()) {
        m_pids.removeAt(index);
    }
}

void AppLauncher::reorderDecks(const QVector<int>& newOrder) {
    if (newOrder.size() != m_pids.size()) {
        return;
    }
    QVector<qint64> reordered;
    reordered.reserve(m_pids.size());
    for (int idx : newOrder) {
        if (idx >= 0 && idx < m_pids.size()) {
            reordered.append(m_pids[idx]);
        } else {
            reordered.append(0);
        }
    }
    m_pids = std::move(reordered);
}
