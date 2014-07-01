#include "connecteddevice.h"

ConnectedDevice::ConnectedDevice(const Device &dev) :
    Device(dev), _socket(this) {
    connect(&_socket, SIGNAL(readyRead()), this, SLOT(onData()));
    connect(&_socket, SIGNAL(connected()), this, SLOT(onConnected()));
    connect(&_socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(onError(QAbstractSocket::SocketError)));
    connect(&_pingTimer, SIGNAL(timeout()), this, SLOT(onPing()));
    connect(&_reconnectTimer, SIGNAL(timeout()), this, SLOT(onReconnectDelayExpired()));

    if(!_ip.isEmpty()){
        qDebug() << "constructor ip not empty";
        QMetaObject::invokeMethod(this, "connectToDevice", Qt::QueuedConnection);
    }
    else {
        qDebug() << "empty ip constructor";
    }
}

ConnectedDevice::~ConnectedDevice(){
    _socket.close();
    _socket.abort();
    _reconnectTimer.stop();
    _pingTimer.stop();
}

void ConnectedDevice::setIp(const QString &ip) {
    if(_ip == ip){
        return;
    }
    _ip = ip;
    _status = ip.isEmpty() ? Device::Unreachable : Device::Located;
    qDebug() << "ip set to " << ip;
    if(ip != ""){
        connectToDevice();
    }
    emit changed();
}

void ConnectedDevice::connectToDevice(){
    qDebug() << "connectToDevice";
    _lastIp = _ip;
    makeConnection();
    setStatus(Device::Connecting);
}

void ConnectedDevice::makeConnection(){
    qDebug() << "starting connection " << _lastIp << " on port " << port();
    _socket.close();
    _socket.abort();
    _pingSent = false;
    _reconnectTimer.stop();
    _pingTimer.stop();
    _socket.connectToHost(_lastIp, port());
}

void ConnectedDevice::onReconnectDelayExpired() {
    makeConnection();
}

void ConnectedDevice::onData() {
    parseInput();
}

void ConnectedDevice::onError(QAbstractSocket::SocketError error) {
    qDebug() << "error: " << error << _socket.errorString();
    closeCnx(true);
}

void ConnectedDevice::closeCnx(bool reconnect) {
    _reconnectTimer.stop();
    _pingTimer.stop();
    _socket.close();
    _socket.abort();
    _pingSent = 0;
    setStatus(Device::Unreachable);
    if(reconnect){
        _reconnectTimer.setSingleShot(true);
        _reconnectTimer.start(reconnectDelay());
    }
}

void ConnectedDevice::onConnected() {
    setStatus(Device::Connected);
    _pingSent = 0;
    _pingTimer.start(pingDelay());
}

void ConnectedDevice::onPing() {
    if(_pingSent > pingLostTolerance()) {
        closeCnx(true);
    }
    else {
        ping();
        ++_pingSent;
    }
}
