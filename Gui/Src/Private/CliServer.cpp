// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#include "../Public/CliServer.h"

#include <Cli/Cli.h>

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

namespace
{
QString pidSuffix()
{
    return "-" + QString::number(QCoreApplication::applicationPid());
}
} // namespace

CliServer::CliServer(UsdStudio::Cli::CliSession& session, QObject* parent)
    : QObject(parent)
    , session_(&session)
{
}

CliServer::~CliServer()
{
    stop();
}

bool CliServer::start(const QString& baseName, QString& activeName, QString& error)
{
    stop();

    std::unique_ptr<QLocalServer> server(new QLocalServer(this));
    // Drop a stale endpoint from a crashed run before claiming the name.
    QLocalServer::removeServer(baseName);
    QString name = baseName;
    if (!server->listen(name))
    {
        // A second GUI instance holds the canonical name; ours gets a pid
        // suffix, which its status bar advertises for '--pipe <name>'.
        name = baseName + pidSuffix();
        QLocalServer::removeServer(name);
        if (!server->listen(name))
        {
            error = server->errorString();
            return false;
        }
    }

    server_ = server.release();
    pipeName_ = name;
    connect(server_, &QLocalServer::newConnection, this, &CliServer::onNewConnection);
    activeName = pipeName_;
    return true;
}

void CliServer::stop()
{
    if (!server_)
        return;
    disconnect(server_, nullptr, this, nullptr);
    server_->close();
    server_->deleteLater();
    server_ = nullptr;
    // Collect keys first: disconnectFromServer() fires disconnected(), which
    // erases from buffers_ while we iterate it.
    const QList<QLocalSocket*> sockets = buffers_.keys();
    for (QLocalSocket* socket : sockets)
    {
        disconnect(socket, nullptr, this, nullptr);
        socket->disconnectFromServer();
    }
    buffers_.clear();
    pipeName_.clear();
}

bool CliServer::isListening() const
{
    return server_ != nullptr && server_->isListening();
}

void CliServer::onNewConnection()
{
    while (QLocalSocket* socket = server_->nextPendingConnection())
    {
        buffers_.insert(socket, QByteArray());
        connect(socket, &QLocalSocket::readyRead, this, &CliServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &CliServer::onDisconnected);
    }
}

void CliServer::onReadyRead()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
        return;

    buffers_[socket].append(socket->readAll());

    int newline = -1;
    while ((newline = buffers_[socket].indexOf('\n')) >= 0)
    {
        const QByteArray line = buffers_[socket].left(newline).trimmed();
        buffers_[socket].remove(0, newline + 1);
        if (line.isEmpty())
            continue;

        // Same evaluator as the CLI process: identical envelope, flags and
        // exit codes. It never throws, so a bad request cannot hurt the GUI.
        const UsdStudio::Cli::CliSession::Result result = session_->evaluate(line.toStdString());
        socket->write(result.json.data(), static_cast<qint64>(result.json.size()));
        socket->write("\n");
        Q_EMIT operationDone();
    }
    socket->flush();
}

void CliServer::onDisconnected()
{
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
        return;
    buffers_.remove(socket);
    socket->deleteLater();
}
