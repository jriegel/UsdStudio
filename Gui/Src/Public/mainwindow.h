// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#pragma once
#include <QMainWindow>
#include <QTime>
#include <QTemporaryDir>
#include <functional>
#include <memory>

class Document;
class CommandRegistry;
class ICommand;


QT_BEGIN_NAMESPACE
class QAction;
class QPlainTextEdit;
class QMenu;
class QTreeView;
class QTableView;
class QLineEdit;
class QTimer;
class QLabel;
class QSlider;
class QListWidget;
QT_END_NAMESPACE

class SceneTreeView;
class UsdStageModel;
class BreadcrumbBar;
class CliServer;
class CommandHistory;
class QueryRegistry;
struct AppContext;
namespace UsdStudio::Cli { class CliSession; }

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
    MainWindow(QString versionString);

    ~MainWindow();

   

public Q_SLOTS:
  void slotTreeContextMenu(const QPoint&);

Q_SIGNALS:

protected:
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);

private Q_SLOTS:
  void newDocument();
  void open();
  void exportPrj();


  void about();

  void view_all();
  void zoom_in();
  void zoom_out();

  

private:
  void createActions();

  void createStatusBar();
  void createDockWindows();

  // Registers every app operation (commands + queries) against the shared
  // registries/context (see AppCommands.cpp in /App - same entry point the
  // CLI and the tests use).
  void registerCommands();

  // Runs the command through the shared CommandHistory; false if it failed.
  bool runCommand(std::unique_ptr<ICommand> command);

  // Syncs window title from the document's current file.
  void updateDocumentUi();

  // Rebuilds the Command History dock from the CommandHistory and updates
  // the Undo/Redo actions.
  void refreshHistoryView();

  // Toggles the '--remote' named-pipe server (live CLI control for agents).
  void startRemoteControl();
  void stopRemoteControl();

  void writeSettings();
  void readSettings();

  void closeEvent(QCloseEvent *event);

  void importFileToProject(QString &fileName, bool visibly, bool insertToSlide);

  int actSelected = 0;
  QTemporaryDir tempDir;
  QString uberCash;
  QString infoDebug;

  SceneTreeView *sceneTreeView = nullptr;
  UsdStageModel *stageModel = nullptr;
  BreadcrumbBar *breadcrumbBar = nullptr;
  QTableView *slidesTableView = nullptr;
  QTableView *animationTableView = nullptr;
  QPlainTextEdit *logList = nullptr;
  QLineEdit *searchInput = nullptr;
  QTimer *searchDebounce = nullptr;

  Document *document = nullptr;
  // The Qt-free CommandHistory is the single undo truth, so menu actions,
  // Ctrl+Z and remote agents (CLI '--remote') share one stack.
  std::unique_ptr<CommandHistory> commandHistory;
  std::unique_ptr<CommandRegistry> commandRegistry;
  std::unique_ptr<QueryRegistry> queryRegistry;
  std::unique_ptr<AppContext> appContext;
  std::unique_ptr<UsdStudio::Cli::CliSession> cliSession;
  std::unique_ptr<CliServer> cliServer;
  QString baseTitle;
  

  QMenu *viewMenu;
 
  bool newIsRunnig = false;
  QAction *importAction = nullptr;
  QAction *exportProjectAction = nullptr;
  QAction *newAction = nullptr;
  QAction *openAction = nullptr;
  QAction *undoAction = nullptr;
  QAction *redoAction = nullptr;
  QAction *remoteControlAction = nullptr;
  QListWidget *historyView = nullptr;
  QLabel *remoteStatusLabel = nullptr;

 
};
