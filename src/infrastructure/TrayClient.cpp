#include "TrayClient.h"
#include <QDebug>
#include <QStringList>

TrayClient::TrayClient(const QString& profileId, QObject* parent)
    : QObject(parent), m_profileId(profileId) {
    connect(&m_socket, &QLocalSocket::readyRead, this, &TrayClient::onReadyRead);
    connect(&m_socket, &QLocalSocket::connected, this, &TrayClient::onConnected);
    connect(&m_socket, &QLocalSocket::disconnected, this, &TrayClient::onDisconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(&m_socket, &QLocalSocket::errorOccurred, this, &TrayClient::onError);
#else
    connect(&m_socket, QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error), this, &TrayClient::onError);
#endif
}

TrayClient::~TrayClient() {
    m_socket.disconnectFromServer();
}

bool TrayClient::connectToDaemon() {
    m_socket.connectToServer("gutterdeck-tray-socket");
    return m_socket.waitForConnected(500);
}

void TrayClient::onConnected() {
    qDebug() << "TrayClient connected to daemon for profile:" << m_profileId;
    QString msg = "REGISTER:" + m_profileId + "\n";
    m_socket.write(msg.toUtf8());
    m_socket.flush();
}

void TrayClient::onDisconnected() {
    qDebug() << "TrayClient disconnected from daemon.";
}

void TrayClient::onError(QLocalSocket::LocalSocketError socketError) {
    qDebug() << "TrayClient error:" << socketError << m_socket.errorString();
}

void TrayClient::onReadyRead() {
    while (m_socket.canReadLine()) {
        QByteArray line = m_socket.readLine().trimmed();
        QString cmd = QString::fromUtf8(line);
        qDebug() << "TrayClient received:" << cmd;
        
        if (cmd == "CMD:NEXT_DECK") emit nextDeckRequested();
        else if (cmd == "CMD:PREV_DECK") emit previousDeckRequested();
        else if (cmd == "CMD:CLOSE_APP") emit closeAppRequested();
        else if (cmd.startsWith("CMD:EDIT_NAME:")) emit editDeckNameRequested(cmd.section(':', 2, 2).toInt());
        else if (cmd.startsWith("CMD:EDIT_CMD:")) emit editDeckCommandRequested(cmd.section(':', 2, 2).toInt());
        else if (cmd.startsWith("CMD:CHANGE_COLOR:")) emit changeDeckColorRequested(cmd.section(':', 2, 2).toInt());
        else if (cmd.startsWith("CMD:ADD_DECK:")) emit addNewDeckRequested(cmd.section(':', 2, 2).toInt());
        else if (cmd.startsWith("CMD:DELETE:")) emit deleteDeckRequested(cmd.section(':', 2, 2).toInt());
    }
}
