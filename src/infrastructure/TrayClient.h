#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QString>

class TrayClient : public QObject {
    Q_OBJECT
public:
    explicit TrayClient(const QString& profileId, QObject* parent = nullptr);
    ~TrayClient() override;

    bool connectToDaemon();

signals:
    void nextDeckRequested();
    void previousDeckRequested();
    void editDeckNameRequested(int index);
    void editDeckCommandRequested(int index);
    void changeDeckColorRequested(int index);
    void addNewDeckRequested(int index);
    void deleteDeckRequested(int index);
    void closeAppRequested();

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QLocalSocket::LocalSocketError socketError);

private:
    QString m_profileId;
    QLocalSocket m_socket;
};
