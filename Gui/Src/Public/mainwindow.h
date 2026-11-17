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
class QUndoStack;
QT_END_NAMESPACE

class SceneTreeView;
class UsdStageModel;
class BreadcrumbBar;

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

  // Registers the Qt-free app commands against the MainWindow's registry
  // and undo stack (see Commands/ in /App).
  void registerCommands();

  // Wraps a command into the QUndoStack; false if it failed.
  bool runCommand(std::unique_ptr<ICommand> command);

  // Syncs window title from the document's current file.
  void updateDocumentUi();

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
  QUndoStack *undoStack = nullptr;
  std::unique_ptr<CommandRegistry> commandRegistry;
  QString baseTitle;
  

  QMenu *viewMenu;
 
  bool newIsRunnig = false;
  QAction *importAction = nullptr;
  QAction *exportProjectAction = nullptr;
  QAction *newAction = nullptr;
  QAction *openAction = nullptr;

 
};
