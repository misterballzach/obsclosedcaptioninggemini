#include "twitch-bot.h"
#include "plugin-main.h"
#include "gemini-client.h"
#include <obs.h>
#include <QRegularExpression>

TwitchBot::TwitchBot(QObject *parent) : QObject(parent), socket(new QSslSocket(this)), isConnected(false)
{
    connect(socket, &QSslSocket::connected, this, &TwitchBot::onConnected);
    connect(socket, &QSslSocket::readyRead, this, &TwitchBot::onReadyRead);
    connect(socket, &QSslSocket::disconnected, this, &TwitchBot::onDisconnected);
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred), this, &TwitchBot::onError);
}

TwitchBot::~TwitchBot()
{
    Disconnect();
}

void TwitchBot::Connect()
{
    if (isConnected || socket->state() != QAbstractSocket::UnconnectedState) return;

    std::string token = GetTwitchToken();
    std::string user = GetTwitchUser();
    std::string channel = GetTwitchChannel();

    if (token.empty() || user.empty() || channel.empty()) {
        blog(LOG_WARNING, "[TwitchBot] Missing credentials, cannot connect.");
        return;
    }

    blog(LOG_INFO, "[TwitchBot] Connecting to Twitch IRC...");
    socket->connectToHostEncrypted("irc.chat.twitch.tv", 6697);
}

void TwitchBot::Disconnect()
{
    if (socket->state() != QAbstractSocket::UnconnectedState) {
        socket->disconnectFromHost();
    }
    isConnected = false;
}

void TwitchBot::onConnected()
{
    blog(LOG_INFO, "[TwitchBot] Connected. Authenticating...");

    std::string token = GetTwitchToken();
    std::string user = GetTwitchUser();
    std::string channel = GetTwitchChannel();

    // Ensure token has "oauth:" prefix
    if (token.find("oauth:") != 0) {
        token = "oauth:" + token;
    }

    QString pass = QString("PASS %1\r\n").arg(QString::fromStdString(token));
    QString nick = QString("NICK %1\r\n").arg(QString::fromStdString(user));
    QString join = QString("JOIN #%1\r\n").arg(QString::fromStdString(channel));

    socket->write(pass.toUtf8());
    socket->write(nick.toUtf8());
    socket->write(join.toUtf8());

    isConnected = true;
}

void TwitchBot::onDisconnected()
{
    blog(LOG_INFO, "[TwitchBot] Disconnected.");
    isConnected = false;
}

void TwitchBot::onError(QAbstractSocket::SocketError socketError)
{
    blog(LOG_ERROR, "[TwitchBot] Socket Error: %s", socket->errorString().toStdString().c_str());
}

void TwitchBot::onReadyRead()
{
    while (socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        processLine(line);
    }
}

void TwitchBot::processLine(const QString &line)
{
    // Handle PING
    if (line.startsWith("PING")) {
        socket->write("PONG :tmi.twitch.tv\r\n");
        return;
    }

    // Parse PRIVMSG
    // Format: :user!user@user.tmi.twitch.tv PRIVMSG #channel :message
    static QRegularExpression msgRegex("^:([^!]+)!.* PRIVMSG #([^ ]+) :(.*)$");
    QRegularExpressionMatch match = msgRegex.match(line);

    if (match.hasMatch()) {
        QString user = match.captured(1);
        QString channel = match.captured(2);
        QString message = match.captured(3);

        if (message.startsWith("!gemini ")) {
            QString prompt = message.mid(8).trimmed();
            if (!prompt.isEmpty()) {
                blog(LOG_INFO, "[TwitchBot] Gemini Request from %s: %s", user.toStdString().c_str(), prompt.toStdString().c_str());

                // Call Gemini API (Async)
                SendTextQueryToGemini(prompt.toStdString(), [this, channel, user](std::string response) {
                    // Back on UI thread? No, SendTextQueryToGemini callback might be on worker thread.
                    // We need to write to socket on the thread socket lives on (Main/UI thread).

                    QString qResponse = QString::fromStdString(response);
                    // Truncate if too long (Twitch limit ~500 chars)
                    if (qResponse.length() > 400) qResponse = qResponse.left(400) + "...";

                    QMetaObject::invokeMethod(this, [this, channel, user, qResponse]() {
                        sendMessage(channel, QString("@%1 %2").arg(user, qResponse));
                    });
                });
            }
        }
    }
}

void TwitchBot::sendMessage(const QString &channel, const QString &message)
{
    if (!isConnected) return;
    QString rawMsg = QString("PRIVMSG #%1 :%2\r\n").arg(channel, message);
    socket->write(rawMsg.toUtf8());
}
