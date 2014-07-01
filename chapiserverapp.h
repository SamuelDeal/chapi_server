#ifndef CHAPISERVERAPP_H
#define CHAPISERVERAPP_H

#include <QApplication>
#include <qlocalserver.h>
#include <models/devicelist.h>
#include <views/apptrayview.h>

class ChapiServerApp : public QApplication
{
    Q_OBJECT
public:
    explicit ChapiServerApp(int &argc, char **argv);
    ~ChapiServerApp();

    void start();

private:
    void openMainWindow();

    QLocalServer _localServer;
    QWidget*_mainWindow;
    DeviceList *_devList;
    AppTrayView *_trayView;

signals:

public slots:
    void onNewLocalConnection();
    void onMainWindowShowAsked();
    void onMainWindowHideAsked();
    void onMainWindowClosed();
    void onLastWindowClosed();
    void onNmapNeeded();
    void onAboutAsked();
    void onExitAsked();
};

#endif // CHAPISERVERAPP_H
