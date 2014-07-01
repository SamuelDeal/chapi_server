#ifndef CHAPIDEVICE_H
#define CHAPIDEVICE_H

#include <QObject>
#include <qtcpsocket.h>
#include <qtimer.h>

#include "connecteddevice.h"
#include "networkconfig.h"

class ChapiDevice: public ConnectedDevice
{
    Q_OBJECT

public:
    enum CnxStatus {
        Unreachable = 1,
        Connecting = 2,
        Connected = 3
    };

    explicit ChapiDevice(const Device &dev);
    virtual ~ChapiDevice();

    void blink();
    void reset();
    void restart();
    NetworkConfig networkConfig();
    bool isConfigurable() const;
    bool isIdentifiable() const;
    Device::DeviceSimpeStatus simpleStatus() const;

private:
    virtual void parseInput();
    virtual void ping();
    virtual quint16 port() const;
    virtual int pingDelay() const;
    virtual quint32 pingLostTolerance() const;
    virtual int reconnectDelay() const;

    void parseLine(const QString &line);
    void updateDeviceStatus();

    CnxStatus _cnxStatus;
    quint8 _nbrBtns;
    NetworkConfig _netCfg;
    bool _configSet;
    QString _targetIp;
    quint64 _targetMac;
    quint8 _outputIndex;
    QMap<quint8, quint8> _inputIndexes;

signals:

public slots:

};

#endif // CHAPIDEVICE_H
