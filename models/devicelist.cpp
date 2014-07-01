#include "devicelist.h"

#include <qdebug.h>
#include <qsettings.h>
#include <qhostaddress.h>
#include <qtimer.h>
#include <qsettings.h>

#include "device.h"
#include "chapidevice.h"
#include "serverdevice.h"
#include "videohubdevice.h"
#include "../utils/netutils.h"

DeviceList::DeviceList() :
    QObject(NULL)
{
    _thread.start();
    _scanner.moveToThread(&_thread);
    connect(&_scanner, SIGNAL(needNmap()), this, SIGNAL(needNmap()));
    connect(&_scanner, SIGNAL(needRoot()), this, SIGNAL(needRoot()));
    connect(&_scanner, SIGNAL(deviceDetected(quint64, QString, QString, quint8)), this, SLOT(onDeviceDetected(quint64, QString, QString, quint8)));
    connect(&_scanner, SIGNAL(allScanFinished()), this, SLOT(onAllScansFinished()));
    QTimer::singleShot(100, &_scanner, SLOT(start()));
}

DeviceList::~DeviceList() {
    foreach(Device *dev, _devices){
        delete dev;
    }
    _thread.quit();
    _thread.wait();
}

void DeviceList::addCurrentCompter() {
    quint64 currentMac = 0;
    foreach(QNetworkInterface netInterface, QNetworkInterface::allInterfaces()) {
        if((netInterface.flags() & QNetworkInterface::IsLoopBack) == 0) {
            currentMac = NetUtils::strToMac(netInterface.hardwareAddress());
            _devices.insert(currentMac, new ServerDevice(Device(currentMac, "Cet Ordinateur", "127.0.0.1", Device::CurrentComputer, Device::ChapiServer), true));
            break;
        }
    }
}

void DeviceList::scanNeedNmap() {
    emit needNmap();
}

void DeviceList::onDeviceDetected(quint64 mac, QString ip, QString name, quint8 type) {
    if(_currentDevScanList.contains(mac)){
        _currentDevScanList[mac] = true;
    }
    else{
        _currentDevScanList.insert(mac, true);
    }

    bool changed = false;
    if(_devices.empty()){
        addCurrentCompter();
        changed = true;
    }

    Device *dev = _devices.value(mac, NULL);

    if(dev == NULL){
        dev = generateDevice(mac, name, ip, ip.isEmpty() ? Device::Unreachable : Device::Located, (Device::DeviceType)type);
        _devices.insert(mac, dev);
        changed = true;
    }
    else{
        if(!name.isEmpty() && dev->name().isEmpty()){
            dev->setName(name);
            changed = true;
        }
        if(ip != dev->ip()){
            dev->setIp(ip);
            changed = true;
        }
        if(dev->status() == Device::Unreachable){
            dev->setStatus(Device::Located);
            changed = true;
        }
    }
    if(changed){
        emit deviceListChanged();
    }
}

void DeviceList::save() const{
    QSettings settings("C:\\Users\\J\\Desktop\\sam\\winctrl.ini", QSettings::IniFormat);
    settings.remove("Devices");
    settings.beginGroup("Devices");
    foreach(Device *dev, _devices) {
        if(!dev->isCurrentComputer()) {
            settings.beginGroup("Device_"+QString::number(dev->mac()));
            settings.setValue("name", dev->name());
            settings.setValue("mac", NetUtils::macToStr(dev->mac()));
            settings.setValue("type", (quint8)dev->type());
            settings.endGroup();
        }
    }
    settings.endGroup();
}

Device* DeviceList::generateDevice(quint64 mac, const QString &name, const QString &ip, Device::DeviceStatus status, Device::DeviceType type) {
    switch(type){
        case Device::ChapiDev:        return new ChapiDevice(Device(mac, name, ip, status, type));
        case Device::ChapiServer:     return new ServerDevice(Device(mac, name, ip, status, type), false);
        case Device::VideoHub:          return new VideoHubDevice(Device(mac, name, ip, status, type));

        case Device::Atem:
        case Device::UnknownDevice:
        default:
            return new Device(mac, name, ip, status, type);
    }
}

void DeviceList::load() {
    QSettings settings("C:\\Users\\J\\Desktop\\sam\\winctrl.ini", QSettings::IniFormat);
    settings.beginGroup("Devices");
    QStringList deviceGroups = settings.childGroups();

    bool changed = false;
    if(_devices.empty()){
        addCurrentCompter();
        changed = true;
    }

    foreach(QString deviceGroup, deviceGroups) {
        settings.beginGroup(deviceGroup);
        QString name = settings.value("name", "").toString();
        QString macStr = settings.value("mac", "").toString();
        quint8 type = settings.value("type", Device::UnknownDevice).toUInt();
        quint64 mac = NetUtils::strToMac(macStr);

        if(mac != 0){
            Device *dev = _devices.value(mac, NULL);
            if(dev == NULL){
                dev = generateDevice(mac, name, "", Device::Unreachable, (Device::DeviceType)type);
                _devices.insert(mac, dev);
                changed = true;
            }
            else{
                if(!name.isEmpty() && dev->name().isEmpty()){
                    dev->setName(name);
                    changed = true;
                }
            }
        }
        settings.endGroup();
    }
    settings.endGroup();

    if(changed){
        emit deviceListChanged();
    }
}

bool DeviceList::containsMac(quint64 mac) const {
    return _devices.contains(mac);
}

struct DeviceSorter {
    bool operator() (const Device* a, const Device* b) const {
        return a->index() > b->index();
    }
};


QList<Device*> DeviceList::devices() const {
    QList<Device*> result;
    foreach(Device* dev, _devices){
        result.push_back(dev);
    }
    qSort(result.begin(), result.end(), [](Device *a, Device *b){
        return a->index() > b->index();
    });
    return result;
}

void DeviceList::setNmapPath(const QString &path) {
    QString pathCopy = path;
    QMetaObject::invokeMethod(&_scanner, "nmapPathDefined", Qt::QueuedConnection, Q_ARG(QString, pathCopy));
}

void DeviceList::onAllScansFinished() {
    foreach(quint64 mac, _devices.keys()) {
        if(!_currentDevScanList.contains(mac) && !_previousDevScanList.contains(mac)) {
            _devices[mac]->setIp("");
        }
    }
    _previousDevScanList = _currentDevScanList;
    _currentDevScanList.clear();
}
