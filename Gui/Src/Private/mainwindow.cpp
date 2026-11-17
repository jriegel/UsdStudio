// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../Public/mainwindow.h"




#include <QtWidgets>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QUndoStack>
#include <QUndoView>
#include <assert.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <array>
#include <span>
#include <string>
#include <string_view>

#include <Document.h>
#include <Commands/CommandRegistry.h>
#include <Commands/DocumentCommands.h>
#include "../Public/UndoCommandAdapter.h"
#include "../Public/UsdStageModel.h"
#include "../Public/SceneTreeView.h"

using namespace std;
using namespace chrono_literals;


MainWindow::MainWindow(QString versionString)
{

 
    //setCentralWidget();

    document = new Document();

    commandRegistry = std::make_unique<CommandRegistry>();
    registerCommands();

    undoStack = new QUndoStack(this);

    createActions();
    createStatusBar();
    createDockWindows();

    baseTitle = tr("UsdStudio ") + versionString + tr( " with USD 22");
    setWindowTitle(baseTitle);

    // Title follows the document after every execute/undo/redo step.
    connect(undoStack, &QUndoStack::indexChanged, this, &MainWindow::updateDocumentUi);
    updateDocumentUi();

    setWindowIcon(QIcon(":/Resources/images/Icon128.png"));

    setUnifiedTitleAndToolBarOnMac(true);

    readSettings();

    setAcceptDrops(true);

    // uses QQmlEngine and QQuickWidget 
    QQmlEngine* engine = new QQmlEngine(this);
    QQuickWidget* view = new QQuickWidget(engine, this);

    view->setSource(QUrl("qrc:/Default.qml"));
    this->setCentralWidget(view);
}

MainWindow::~MainWindow() 
{
    if (document) delete document;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  

    writeSettings();
    event->accept();
  
}


void  MainWindow::newDocument()
{
  runCommand(commandRegistry->create("document.new", std::span<const std::string_view>{}));
}





void MainWindow::open()
{
  QString fileName = QFileDialog::getOpenFileName(this,
    "Open USD file", nullptr,
    "USD files (*.usd *.usda *.usdc *.usdz);;All files (*.*)");

  if (fileName.isEmpty())
    return;

  try
  {
    std::string fileNameStd = fileName.toStdString();
    std::array<std::string_view, 1> args{ std::string_view(fileNameStd) };
    if (runCommand(commandRegistry->create("document.open", std::span(args))))
      statusBar()->showMessage(tr("Opened document: '%1'").arg(fileName), 3000);
  }
  catch (const std::exception& e)
  {
    statusBar()->showMessage(tr("Could not open '%1': %2").arg(fileName, e.what()), 5000);
  }
}



void MainWindow::registerCommands()
{
  commandRegistry->registerCommand<NewDocumentCommand, Document>(
      "document.new", "Replaces the document with a new empty stage.", document);
  commandRegistry->registerCommand<OpenDocumentCommand, Document, std::string>(
      "document.open", "Opens a USD file: document.open <fileName>", document);
}



bool MainWindow::runCommand(std::unique_ptr<ICommand> command)
{
  if (!command)
    return false;

  try
  {
    // Execute before pushing: if execute() throws, the undo stack stays
    // untouched. The adapter skips the redundant execution that
    // QUndoStack::push triggers.
    command->execute();
    undoStack->push(new UndoCommandAdapter(std::move(command)));
    return true;
  }
  catch (const std::exception& e)
  {
    statusBar()->showMessage(tr("Command failed: %1").arg(e.what()), 5000);
    return false;
  }
}



void MainWindow::updateDocumentUi()
{
  const std::string file = document ? document->getFileName() : std::string();
  if (file.empty())
    setWindowTitle(baseTitle);
  else
    setWindowTitle(baseTitle + " - " + QFileInfo(QString::fromStdString(file)).fileName());

  // The scene graph dock follows the document: execute, undo and redo all
  // end up here, so the tree never shows a stale stage.
  if (stageModel)
  {
    stageModel->setStage(document ? document->getStage() : pxr::UsdStageRefPtr());
    statusBar()->showMessage(tr("%1 top level prim(s)").arg(stageModel->rowCount()), 3000);
  }
}

void MainWindow::exportPrj()
{
    QMimeDatabase mimeDatabase;
    QString fileName = QFileDialog::getSaveFileName(this,
        "Save project", nullptr, "PLMXML (*.plmxml);;Monolythic JT (*.jt);;Uber NOV file (*.nov);;All formats (*.plmxml  *.nov *.jt)");

    if (fileName.isEmpty())
        return;

    //exportProject(fileName);

    statusBar()->showMessage(tr("Start saving project: '%1'").arg(fileName), 2000);
}





void  MainWindow::importFileToProject(QString &fileName, bool visibly, bool insertToSlide)
{
    
}

void MainWindow::about()
{
  QMessageBox::about(this, "About Mercedes Benz Tech Motion USD-Tool",
    "<b>USD-Tool</b> is a simple Plmxml/Jt Viewer application to export USD.<br/><br/>"
      "<br/>"
    " (c) 2022 Mercedes Benz Tech Motion <br/>"
    " Contact: juergen.riegel@mercedes-benz.com "
  );
}

void MainWindow::view_all()
{
  //if (actSelected == 0)
    //uberWidget->zoomToFit();
  //else
    //uberWidget->zoomToFit(actSelected);
}

void MainWindow::zoom_in()
{
  //uberWidget->zoomIn();
}

void MainWindow::zoom_out()
{
  //uberWidget->zoomOut();
}






void MainWindow::createActions()
{
// =========================================================================================================
// ===== File  ===========================================================================================
// =========================================================================================================

  QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
  QToolBar *fileToolBar = addToolBar(tr("File"));

  const QIcon newIcon = QIcon::fromTheme("document-new", QIcon(":/Resources/images/new.png"));
  newAction = new QAction(newIcon, tr("&New"), this);
  newAction->setShortcuts(QKeySequence::New);
  newAction->setStatusTip(tr("Create a new project/scene"));
  newAction->setEnabled(true);
  connect(newAction, &QAction::triggered, this, &MainWindow::newDocument);
  fileMenu->addAction(newAction);
  fileToolBar->addAction(newAction);

  
  const QIcon openIcon = QIcon::fromTheme("document-open", QIcon(":/Resources/images/open.png"));
  openAction = new QAction(openIcon, tr("&Open..."), this);
  openAction->setShortcuts(QKeySequence::Open);
  openAction->setStatusTip(tr("Open a project file"));
  openAction->setEnabled(true);
  connect(openAction, &QAction::triggered, this, &MainWindow::open);
  fileMenu->addAction(openAction);
  fileToolBar->addAction(openAction);
  
  

  fileMenu->addSeparator();

  QAction *quitAct = fileMenu->addAction(tr("&Quit"), this, &QWidget::close);
  quitAct->setShortcuts(QKeySequence::Quit);
  quitAct->setStatusTip(tr("Quit the application"));
  quitAct->setEnabled(true);



// =========================================================================================================
// ===== Edit    ===========================================================================================
// =========================================================================================================

  QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
  QToolBar *editToolBar = addToolBar(tr("Edit"));

  QAction *undoAction = undoStack->createUndoAction(this, tr("Undo"));
  undoAction->setShortcuts(QKeySequence::Undo);
  undoAction->setStatusTip(tr("Undo the last command"));
  editMenu->addAction(undoAction);
  editToolBar->addAction(undoAction);

  QAction *redoAction = undoStack->createRedoAction(this, tr("Redo"));
  redoAction->setShortcuts(QKeySequence::Redo);
  redoAction->setStatusTip(tr("Redo the last undone command"));
  editMenu->addAction(redoAction);
  editToolBar->addAction(redoAction);



// =========================================================================================================
// ===== View    ===========================================================================================
// =========================================================================================================

/*
  QToolBar *viewToolBar = addToolBar(tr("View"));
  viewMenu = menuBar()->addMenu(tr("&View"));

  const QIcon viewAllIcon = QIcon::fromTheme("view-viewall", QIcon(":/Resources/images/resize.png"));
  QAction *viewAllAction = new QAction(viewAllIcon, tr("&View all"), this);
  viewAllAction->setStatusTip(tr("Fit the whole scene to the screen"));
  viewAllAction->setEnabled(true);
  connect(viewAllAction, &QAction::triggered, this, &MainWindow::view_all);
  viewMenu->addAction(viewAllAction);
  viewToolBar->addAction(viewAllAction);

  const QIcon zoomInIcon = QIcon::fromTheme("view-zoomin", QIcon(":/Resources/images/plus.png"));
  QAction *zoomInAction = new QAction(zoomInIcon, tr("&Zoom in"), this);
  zoomInAction->setStatusTip(tr("Zoom one step in"));
  zoomInAction->setEnabled(true);
  connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoom_in);
  viewMenu->addAction(zoomInAction);
  viewToolBar->addAction(zoomInAction);

  const QIcon zoomOutIcon = QIcon::fromTheme("view-zoomin", QIcon(":/Resources/images/minus.png"));
  QAction *zoomOutAction = new QAction(zoomOutIcon, tr("&Zoom out"), this);
  zoomOutAction->setStatusTip(tr("Zoom one step out"));
  zoomOutAction->setEnabled(true);
  connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoom_out);
  viewMenu->addAction(zoomOutAction);
  viewToolBar->addAction(zoomOutAction);

  QAction *newAction = LayerIgnoreAction = new QAction("Reset all transform", this);
  connect(newAction, &QAction::triggered, this, [this]() {
      uberSceneCache->resetAllTransformation();
  });
  viewMenu->addAction(newAction);

  viewMenu->addSeparator();
  */



  
  menuBar()->addSeparator();

  QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

  QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
  aboutAct->setStatusTip(tr("Show the application's About box"));
}



void MainWindow::createStatusBar()
{
  statusBar()->showMessage(tr("Ready"));
}

void MainWindow::createDockWindows()
{
  // Tree View ==============================================================================================
  // UsdStageModel feeds the view lazily (see its header): nothing is built
  // beyond the chunk the user actually sees.
  stageModel = new UsdStageModel(this);

  QWidget *wrapper = new QWidget();
  QDockWidget *dock = new QDockWidget(tr("Scene Graph"), this);
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  QVBoxLayout *mainLayout = new QVBoxLayout();
  QHBoxLayout *hLayout = new QHBoxLayout;
  searchInput = new QLineEdit();
  searchInput->setPlaceholderText(tr("Filter by name or type"));
  searchInput->setClearButtonEnabled(true);
  hLayout->addWidget(searchInput);
  mainLayout->addLayout(hLayout);

  breadcrumbBar = new BreadcrumbBar(wrapper);
  mainLayout->addWidget(breadcrumbBar);

  sceneTreeView = new SceneTreeView();
  sceneTreeView->setModel(stageModel);
  sceneTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
  mainLayout->addWidget(sceneTreeView);

  breadcrumbBar->setTreeView(sceneTreeView);

  wrapper->setLayout(mainLayout);
  dock->setWidget(wrapper);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  dock->setVisible(true);
  //viewMenu->addAction(dock->toggleViewAction());

  // Debounced filter: typing must not walk the stage on every keystroke.
  searchDebounce = new QTimer(this);
  searchDebounce->setSingleShot(true);
  searchDebounce->setInterval(150);
  connect(searchDebounce, &QTimer::timeout, this, [this]() {
    stageModel->setFilterText(searchInput->text());
    const int matches = stageModel->matchCount();
    if (!searchInput->text().isEmpty())
      statusBar()->showMessage(tr("%1 matching prim(s)").arg(matches), 3000);
  });
  connect(searchInput, &QLineEdit::textChanged, this, [this]() { searchDebounce->start(); });

  QObject::connect(sceneTreeView, &QWidget::customContextMenuRequested, this, &MainWindow::slotTreeContextMenu);

  
  // Command History ==============================================================================================
  QDockWidget *historyDock = new QDockWidget(tr("Command History"), this);
  historyDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
  historyDock->setWidget(new QUndoView(undoStack, historyDock));
  addDockWidget(Qt::BottomDockWidgetArea, historyDock);
  historyDock->setVisible(true);

}

void MainWindow::writeSettings()
{
  QSettings settings("UsdStudio", "UsdStudio");

  settings.beginGroup("MainWindow");
  settings.setValue("size", size());
  settings.setValue("pos", pos());
  settings.endGroup();
}

void MainWindow::readSettings()
{
  QSettings settings("UsdStudio", "UsdStudio");

  settings.beginGroup("MainWindow");
  resize(settings.value("size", QSize(400, 400)).toSize());
  move(settings.value("pos", QPoint(200, 200)).toPoint());
  settings.endGroup();
}

void MainWindow::slotTreeContextMenu(const QPoint &point)
{
  QModelIndex index = sceneTreeView->indexAt(point);
  
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls() ) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event)
{

    for (const QUrl& url : event->mimeData()->urls())
    {
        QString fileName = url.toLocalFile();
        QFileInfo file(fileName);
    }
  
}