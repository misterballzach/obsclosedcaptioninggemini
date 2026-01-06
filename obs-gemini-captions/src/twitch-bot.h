#pragma once

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QTimer>

class TwitchBot : public QObject {
    Q_OBJECT

public:
    explicit TwitchBot(QObject *parent = nullptr);
    ~TwitchBot();

    void Connect();
    void Disconnect();

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError socketError);

private:
    QSslSocket *socket;
    bool isConnected;

    void processLine(const QString &line);
    void sendMessage(const QString &channel, const QString &message);
};
