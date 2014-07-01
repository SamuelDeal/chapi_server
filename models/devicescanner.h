#ifndef DEVICESCANNER_H
#define DEVICESCANNER_H

#include <qobject.h>
#include <qstringlist.h>
#include <qmap.h>
#include <qhostaddress.h>
#include <qprocess.h>
#include <QDomNode>
#include <qudpsocket.h>
#include <qtimer.h>

#include "device.h"

class DeviceScanner : public QObject
{
    Q_OBJECT
public:
    explicit DeviceScanner();
    ~DeviceScanner();

private:
    bool checkNmap(const QString &path);
    Device::DeviceType guessType(bool cfgPort, bool vhPort, bool atemPort, bool serverPort, const QString &ip);
    void parseScanResult(const QDomNode &devNode);
    void cleanProc(QProcess *proc);

    QString _nmapPath;
    QStringList _defaultPaths;
    QMap<quint32, QPair<QHostAddress, int> > _networks;
    QMap<quint32, QProcess*> _scanners;
    QUdpSocket _udpSocket;
    QTimer _timer;

signals:
    void needNmap();
    void needRoot();
    void onScanResult();
    void allScanFinished();
    void deviceDetected(quint64 mac, QString ip, QString name, quint8 type);

public slots:
    void start();
    void sayHello();
    void nmapPathDefined(QString path);
    void scan();
    void error(QProcess::ProcessError err);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void helloReceived();
};

#endif // DEVICESCANNER_H
