// (c)2024 Juergen Riegel, mail@juergen-riegel.net
// Headless test of the GUI remote-control server: a QLocalSocket client sends
// operation lines and must receive the same JSON envelopes the CLI produces.

#include "CliServer.h"

#include <AppContext.h>
#include <Commands/AppCommands.h>
#include <Commands/CommandHistory.h>
#include <Commands/CommandRegistry.h>
#include <Cli/Cli.h>
#include <Document.h>
#include <Commands/QueryRegistry.h>

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QLocalSocket>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QTemporaryDir>

#include <iostream>
#include <string>
#include <vector>

namespace
{
int failures = 0;
int checks = 0;

void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::cout << "FAIL: " << what << '\n';
    }
}

// Pump the event loop until 'predicate' holds or the deadline passes.
template <typename Predicate>
bool waitFor(Predicate&& predicate, int timeoutMs = 5000)
{
    const QDeadlineTimer deadline(timeoutMs);
    while (!predicate())
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        if (deadline.hasExpired())
            break;
    }
    return predicate();
}

// One request/response round trip; returns the response line.
bool request(QLocalSocket& socket, const std::string& line, std::string& responseOut)
{
    socket.write(QByteArray(line) + '\n');
    socket.flush();
    if (!waitFor([&]() { return socket.canReadLine(); }))
        return false;
    responseOut = socket.readLine().toStdString();
    return true;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}
} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Unique pipe name per run so parallel runs or crashed leftovers never collide.
    const QString pipeName =
        QStringLiteral("usdstudio-cliserver-test-%1").arg(QCoreApplication::applicationPid());

    Document document;
    CommandHistory history;
    CommandRegistry commands;
    QueryRegistry queries;
    AppContext context{document, history};
    registerAppCommands(commands, queries, context);
    UsdStudio::Cli::CliSession session(context, commands, queries);

    CliServer server(session);
    QString activeName;
    QString error;
    check(server.start(pipeName, activeName, error), "server starts listening: " + error.toStdString());
    check(activeName == pipeName, "server reports the requested pipe name");

    CliServer secondServer(session);
    QString secondName;
    check(secondServer.start(pipeName, secondName, error), "second server starts: " + error.toStdString());
    check(secondName != pipeName, "second server uses a distinct pipe name");

    QLocalSocket socket;
    socket.connectToServer(pipeName);
    check(socket.waitForConnected(3000), "client connects to the pipe");

    std::string response;

    // 1. Read-only query against the live (empty) document.
    if (request(socket, "stage.info", response))
    {
        check(contains(response, "\"ok\":true"), "stage.info returns ok envelope");
        check(contains(response, "\"exit\":0"), "stage.info reports exit 0");
    }
    else
        check(false, "stage.info got a response");

    // 2. Unknown operation -> structured error, non-zero exit code.
    if (request(socket, "bogus.operation", response))
    {
        check(contains(response, "\"ok\":false"), "unknown operation returns error envelope");
        check(contains(response, "\"code\":\"usage\""), "unknown operation reports the usage code");
        check(contains(response, "Unknown operation"), "unknown operation is named");
        check(contains(response, "\"exit\":2"), "unknown operation reports exit 2");
    }
    else
        check(false, "bogus.operation got a response");

    // 3. Commands run through the shared history; a later query sees them.
    if (request(socket, "document.new", response))
        check(contains(response, "\"ok\":true"), "document.new succeeds remotely");
    else
        check(false, "document.new got a response");

    if (request(socket, "history.list", response))
    {
        check(contains(response, "\"ok\":true"), "history.list succeeds");
        check(contains(response, "New document"), "history shows the remotely executed command");
    }
    else
        check(false, "history.list got a response");

    check(history.undoDepth() == 1 && history.canUndo(), "GUI-side history received the remote command");

    QProcess cli;
    cli.start(QString::fromUtf8(USDSTUDIO_CMD_PATH),
              {QStringLiteral("--remote"), QStringLiteral("--pipe"), pipeName,
               QStringLiteral("--command"), QStringLiteral("stage.info"),
               QStringLiteral("--command"), QStringLiteral("history.list")});
    check(waitFor([&]() { return cli.state() == QProcess::NotRunning; }),
          "CLI remote process completes");
    const std::string cliOutput = cli.readAllStandardOutput().toStdString();
    check(cli.exitStatus() == QProcess::NormalExit && cli.exitCode() == 0,
          "CLI remote process succeeds");
    check(contains(cliOutput, "stage.info") && contains(cliOutput, "New document"),
          "CLI connects to the original server and receives both responses");
    secondServer.stop();

#ifndef _WIN32
    QTemporaryDir directory;
    check(directory.isValid(), "temporary socket directory created");
    CliServer customServer(session);
    QString customName;
    check(customServer.start(directory.path() + "/custom", customName, error),
          "server starts at an absolute socket path");
    QProcess customCli;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("TMPDIR"), directory.path());
    customCli.setProcessEnvironment(environment);
    customCli.start(QString::fromUtf8(USDSTUDIO_CMD_PATH),
                    {QStringLiteral("--remote"), QStringLiteral("--pipe"),
                     QStringLiteral("custom"), QStringLiteral("stage.info")});
    check(waitFor([&]() { return customCli.state() == QProcess::NotRunning; }),
          "CLI connects using TMPDIR");
    check(customCli.exitStatus() == QProcess::NormalExit && customCli.exitCode() == 0 &&
              contains(customCli.readAllStandardOutput().toStdString(), "\"exit\":0"),
          "CLI receives a response through the custom socket directory");
    customServer.stop();
#endif

    // 4. Two lines in one write must produce two response lines (framing).
    socket.write("stage.info\nhistory.list\n");
    socket.flush();
    std::vector<std::string> lines;
    waitFor([&]() {
        while (socket.canReadLine())
            lines.push_back(socket.readLine().toStdString());
        return lines.size() >= 2;
    });
    check(lines.size() == 2, "pipelined requests yield one response line each");
    if (lines.size() == 2)
    {
        check(contains(lines[0], "stage.info"), "first response belongs to first request");
        check(contains(lines[1], "New document"), "second response belongs to second request");
    }

    // 5. stop() closes the pipe; new clients must fail.
    server.stop();
    check(!server.isListening(), "server stops listening");
    QLocalSocket refused;
    refused.connectToServer(pipeName);
    check(!refused.waitForConnected(1500), "connection refused after stop()");

    std::cout << (checks - failures) << "/" << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}
