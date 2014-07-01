#include "chapiserverapp.h"

#include <qlocalsocket.h>
#include <QTranslator>
#include <QLibraryInfo>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <functional>

#include "views/nmappathview.h"
#include "views/mainview.h"
#include "utils/sighandler.h"


//TODO: rename hatuin to Chapi : Controller de Hub Audio-vidéo
//Par Informatique
//Presque Indolore
//Provenant d'Insomnies
//Plutot Inovant/Ingénieux
//Prétenduement instinctif
//prodigieusement itinérant

//TODO check for translations :Qt bug?

#ifdef _WIN32
class WindowsEventFilter : public QAbstractNativeEventFilter {
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result);
};

bool WindowsEventFilter::nativeEventFilter(const QByteArray &eventType, void *message, long *result) {
    Q_UNUSED(eventType);
    Q_UNUSED(result);

    //TODO: ask on the Qt mailing list if it's not a bug
    //TODO: check for event of kill windows
    MSG* msg = (MSG*)(message);
    if((msg->message == WM_CLOSE) && (msg->hwnd == 0)){
        ChapiServerApp *app = static_cast<ChapiServerApp*>(ChapiServerApp::instance());
        QMetaObject::invokeMethod(app, "onExitAsked", Qt::QueuedConnection);
        return true;
    }

    return false;
}
#endif // _WIN32

ChapiServerApp::ChapiServerApp(int &argc, char **argv) :
    QApplication(argc, argv), _localServer(this)
{
    _mainWindow = NULL;
    _devList = NULL;
    _trayView = NULL;

    QLocalSocket socket;
    socket.connectToServer("ChapiServer");
    if (socket.waitForConnected(300)) { //previous instance found!
        socket.write("PING\n");
        socket.flush();
        socket.close();
        QTimer::singleShot(1, this, SLOT(quit()));
        return;
    }

    connect(&_localServer, SIGNAL(newConnection()), this, SLOT(onNewLocalConnection()));
    _localServer.listen("ChapiServer");

    setWindowIcon(QIcon(":/icons/chapi.png"));

    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    QTranslator *translator = new QTranslator();
    QString lang = QLocale::system().name();
    lang.truncate(2);
    if(translator->load("qt_"+lang+".qm", ":/i18n")){
        installTranslator(translator);
    };


    connect(this, SIGNAL(lastWindowClosed()), this, SLOT(onLastWindowClosed()));
    QApplication::setQuitOnLastWindowClosed(false);

    _devList = new DeviceList;
    connect(_devList, SIGNAL(needNmap()), this, SLOT(onNmapNeeded()));
    _devList->load();

    _trayView = new AppTrayView();
    connect(_trayView, SIGNAL(mainWindowShowCmd()), this, SLOT(onMainWindowShowAsked()));
    connect(_trayView, SIGNAL(mainWindowHideCmd()), this, SLOT(onMainWindowHideAsked()));
    connect(_trayView, SIGNAL(aboutCmd()), this, SLOT(onAboutAsked()));
    connect(_trayView, SIGNAL(exitCmd()), this, SLOT(onExitAsked()));

    //TODO checkThat

    SigHandler::get().addCallback([]{
        qDebug() << "event catched";
        ChapiServerApp *app = static_cast<ChapiServerApp*>(ChapiServerApp::instance());
        QMetaObject::invokeMethod(app, "onExitAsked", Qt::QueuedConnection);
        return true;
    });

#ifdef _WIN32
    QCoreApplication::eventDispatcher()->installNativeEventFilter(new WindowsEventFilter());
#endif
    openMainWindow();
}


ChapiServerApp::~ChapiServerApp() {
    if(_devList != NULL){
        _devList->save();
        delete _devList;
    }
    if(_trayView != NULL){
        delete _trayView;
    }
}

void ChapiServerApp::onMainWindowShowAsked() {
    openMainWindow();
}

void ChapiServerApp::onMainWindowHideAsked() {
    if(_mainWindow == NULL){
        return;
    }
    _mainWindow->close();
}

void ChapiServerApp::openMainWindow() {
    if(_mainWindow != NULL){
        return;
    }
    _mainWindow = new MainView(_devList);
    _mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    connect(_mainWindow, SIGNAL(destroyed()), this, SLOT(onMainWindowClosed()));
    _mainWindow->show();
    setActiveWindow(_mainWindow);
    _mainWindow->setFocus();
    _trayView->onMainWindowVisibilityChanged(true);
}

void ChapiServerApp::onMainWindowClosed() {
    _mainWindow = NULL;
    _trayView->onMainWindowVisibilityChanged(false);
}

void ChapiServerApp::onNewLocalConnection() {
    QLocalSocket *localSocket = _localServer.nextPendingConnection();
    connect(localSocket, SIGNAL(disconnected()), localSocket, SLOT(deleteLater()));

    while (localSocket->bytesAvailable() == 0) {
        localSocket->waitForReadyRead();
    }
    localSocket->disconnectFromServer();
    openMainWindow();
    _mainWindow->setFocus();
}

void ChapiServerApp::onLastWindowClosed() {
    _trayView->displayTooltip();
}

void ChapiServerApp::onNmapNeeded() {
    NmapPathView nmapView(_mainWindow);
    nmapView.exec();

    if(nmapView.isCanceled()){
        QApplication::quit();
    }
    else {
        _devList->setNmapPath(nmapView.newPath());
    }
}

void ChapiServerApp::onAboutAsked() {
    //TODO: description du about
    QMessageBox::about(_mainWindow, "A propos de Chapi Serveur", "Description\nsur\nplusieurs\nlignes\n<a href='http://www.google.com'>test lien</a>");
}

void ChapiServerApp::onExitAsked(){
    closeAllWindows();
    quit();
}
