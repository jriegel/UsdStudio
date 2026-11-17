// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#include "../Public/mainwindow.h"




#include <QtWidgets>
#include <QQmlEngine>
#include <QQuickWidget>
#include <assert.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <filesystem>

#include <Document.h>

using namespace std;
using namespace chrono_literals;


MainWindow::MainWindow(QString versionString)
{

 
    //setCentralWidget();

    document = new Document();

    createActions();
    createStatusBar();
    createDockWindows();


    setWindowTitle(tr("UsdStudio " ) + versionString + tr( " with USD 22"));

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
  if (document)
    document->New();
  else
    document = new Document();

  setWindowTitle(tr("UsdStudio"));
  statusBar()->showMessage(tr("New document created"), 3000);
}





void MainWindow::open()
{
  QString fileName = QFileDialog::getOpenFileName(this,
    "Open USD file", nullptr,
    "USD files (*.usd *.usda *.usdc *.usdz);;All files (*.*)");

  if (fileName.isEmpty())
    return;

  if (!document)
    document = new Document();

  if (!document->Open(fileName.toStdString()))
  {
    statusBar()->showMessage(tr("Failed to open project: '%1'").arg(fileName), 5000);
    return;
  }

  setWindowTitle(tr("UsdStudio - %1").arg(QFileInfo(fileName).fileName()));
  statusBar()->showMessage(tr("Opened project: '%1'").arg(fileName), 3000);
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
  QWidget *wrapper = new QWidget();
  QDockWidget *dock = new QDockWidget(tr("Scene Graph"), this);
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  QVBoxLayout *mainLayout = new QVBoxLayout();
  QHBoxLayout *hLayout = new QHBoxLayout;
  searchInput = new QLineEdit();
  hLayout->addWidget(searchInput);
  QPushButton *buttonDel = new QPushButton();
  buttonDel->setText("X");
  hLayout->addWidget(buttonDel);
  mainLayout->addLayout(hLayout);
  sceneTreeView = new QTreeView();
  sceneTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
  mainLayout->addWidget(sceneTreeView);

  wrapper->setLayout(mainLayout);
  dock->setWidget(wrapper);
  addDockWidget(Qt::LeftDockWidgetArea, dock);
  dock->setVisible(true);
  //viewMenu->addAction(dock->toggleViewAction());
  //QObject::connect(searchInput, &QLineEdit::textChanged, this, &MainWindow::text_changed);
  //QObject::connect(buttonDel, &QPushButton::released, this, &MainWindow::text_delete);
  QObject::connect(sceneTreeView, &QWidget::customContextMenuRequested, this, &MainWindow::slotTreeContextMenu);

  
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