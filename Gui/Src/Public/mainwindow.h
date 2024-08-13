// (c)2024 Juergen Riegel, mail@juergen-riegel.net

#pragma once
#include <QMainWindow>
#include <QTime>
#include <QTemporaryDir>
#include <functional>


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
QT_END_NAMESPACE

class TreeModel;
class ModelAdaptor;


class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
    MainWindow(QString versionString);

    ~MainWindow();

   

public slots:
  void slotTreeContextMenu(const QPoint&);

signals:

protected:
  void dragEnterEvent(QDragEnterEvent *event);
  void dropEvent(QDropEvent *event);

private slots:
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

  void writeSettings();
  void readSettings();

  void closeEvent(QCloseEvent *event);

  void importFileToProject(QString &fileName, bool visibly, bool insertToSlide);

  int actSelected = 0;
  QTemporaryDir tempDir;
  QString uberCash;
  QString infoDebug;

  QTreeView *sceneTreeView;
  QTableView *slidesTableView;
  QTableView *animationTableView;
  QPlainTextEdit *logList;
  QLineEdit *searchInput;


  

  QMenu *viewMenu;
 
  bool newIsRunnig = false;
  QAction *importAction = nullptr;
  QAction *exportProjectAction = nullptr;
  QAction *newAction = nullptr;
  QAction *openAction = nullptr;

 
};
