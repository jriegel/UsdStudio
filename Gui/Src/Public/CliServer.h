// (c)2024 Juergen Riegel, mail@juergen-riegel.net
#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;
class QLocalSocket;

namespace UsdStudio::Cli
{
class CliSession;
}

// Remote control server inside the GUI process. It listens on a local named
// pipe; 'UsdStudioCmd --remote' (typically driven by an AI agent) sends one
// operation line per request and receives one JSON envelope line back, all
// executed against the live document and the shared undo history.
class CliServer : public QObject
{
    Q_OBJECT
public:
    explicit CliServer(UsdStudio::Cli::CliSession& session, QObject* parent = nullptr);
    ~CliServer() override;

    // Listens on 'baseName'. If another live instance owns that name, falls
    // back to "<baseName>-<pid>" so several GUIs can be targeted separately.
    // On success 'activeName' receives the name the client has to use.
    bool start(const QString& baseName, QString& activeName, QString& error);
    void stop();
    bool isListening() const;
    QString pipeName() const { return pipeName_; }

Q_SIGNALS:
    // Emitted after every remote operation so the UI can refresh (undo view,
    // title, scene model, ...).
    void operationDone();

private:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

    UsdStudio::Cli::CliSession* session_;
    QLocalServer* server_ = nullptr;
    QHash<QLocalSocket*, QByteArray> buffers_; // per-socket partial lines
    QString pipeName_;
};
