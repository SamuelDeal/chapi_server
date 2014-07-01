#include "videohubdevice.h"

#include <QDebug>

#include "../const.h"

VideoHubDevice::VideoHubDevice(const Device &dev) :
    ConnectedDevice(dev), _protocol(&_socket) {
    _blinking = false;
    connect(&_blinkTimer, SIGNAL(timeout()), this, SLOT(onBlinked()));
    connect(&_protocol, SIGNAL(onCommandFailed(NlCommand)), this, SLOT(onCommandFailed(NlCommand)));
    connect(&_protocol, SIGNAL(onCommandReceived(NlCommand)), this, SLOT(onCommandReceived(NlCommand)));
}

VideoHubDevice::~VideoHubDevice(){
}

quint16 VideoHubDevice::port() const {
    return VIDEOHUB_PORT;
}

int VideoHubDevice::pingDelay() const {
    return 50000;
}

quint32 VideoHubDevice::pingLostTolerance() const {
    return 2;
}
int VideoHubDevice::reconnectDelay() const {
    return 2000;
}

bool VideoHubDevice::isConfigurable() const {
    return true;
}

bool VideoHubDevice::isIdentifiable() const {
    return true;
}

bool VideoHubDevice::isConfigurableNow() const {
    return _status == Device::Ready;
}

bool VideoHubDevice::isIdentifiableNow() const {
    return _status == Device::Ready;
}

void VideoHubDevice::blink() {
    if(_blinking){
        return;
    }
    _blinking = true;
    _blinkTimer.start(450);
    QTimer::singleShot(2000, Qt::PreciseTimer, this, SLOT(onBlinkFinished()));
}

void VideoHubDevice::onBlinkFinished() {
    QStringList cmds;
    foreach(quint8 output, _outputLabels.keys()) {
        cmds.push_back(QString::number(output)+" "+QString::number(_routingTable[output]));
    }
    _protocol.sendCommand("VIDEO OUTPUT ROUTING", cmds);
    _blinkTimer.stop();
    _blinking = false;
}

void VideoHubDevice::onBlinked() {
    QStringList cmds;
    foreach(quint8 output, _outputLabels.keys()) {
        foreach (quint8 input, _inputLabels.keys()) {
            cmds.push_back(QString::number(output)+" "+QString::number(input));
        }
    }
    _protocol.sendCommand("VIDEO OUTPUT ROUTING", cmds);
}

Device::DeviceSimpeStatus VideoHubDevice::simpleStatus() const {
    if((_status == Unreachable) || (_status == Device::Located)){
        return Device::Red;
    }
    if(_status == Device::Ready) {
        return Device::Green;
    }
    return Device::Yellow;
}

void VideoHubDevice::parseInput(){
    _protocol.read();
}

void VideoHubDevice::ping(){
    _protocol.sendCommand("VIDEO OUTPUT LOCKS");
}

void VideoHubDevice::checkEndConfig() {
    if((_status != Device::ReadingConfig) || (_inputLabels.size() == 0) || (_outputLabels.size() == 0)){
        return;
    }
    if((_outputLabels.size() != _routingTable.size())){
        return;
    }
    setStatus(Device::Ready);
}
QMap<quint8, QString> VideoHubDevice::outputLabels() const {
    return _outputLabels;
}


QMap<quint8, QString> VideoHubDevice::inputLabels() const {
    return _inputLabels;
}

void VideoHubDevice::setInputName(quint8 index, QString name) {
    _protocol.sendCommand("INPUT LABELS", QString::number(index)+" "+name);
}

void VideoHubDevice::setOutputName(quint8 index, QString name) {
    _protocol.sendCommand("OUTPUT LABELS", QString::number(index)+" "+name);
}

void VideoHubDevice::onCommandFailed(NlCommand cmd){
    qDebug() << "command " << cmd.command << " failed";
    foreach(QString line, cmd.lines){
        qDebug() << line;
    }
}


void VideoHubDevice::onCommandReceived(NlCommand cmd){
    _pingSent = 0;

    if(cmd.command == "PROTOCOL PREAMBLE"){
        setStatus(Device::ReadingConfig);
        QRegExp regex("^Version: +([0-9]+\\.[0-9]+)$");
        if(regex.exactMatch(cmd.lines[0])){
            _version = regex.cap(1);
        }
        _inputLabels.clear();
        _outputLabels.clear();
        _routingTable.clear();
    }
    else if(cmd.command == "INPUT LABELS"){
        QRegExp regex("^([0-9]+) +(.*) *$");
        foreach(QString line, cmd.lines) {
            if(regex.exactMatch(line)){
                quint8 index = regex.cap(1).toUShort();
                if(_inputLabels.contains(index)){
                    _inputLabels[index] = regex.cap(2);
                }
                else{
                    _inputLabels.insert(index, regex.cap(2));
                }
            }
        }
        checkEndConfig();
        emit changed();
    }
    else if(cmd.command == "OUTPUT LABELS"){
        QRegExp regex("^([0-9]+) +(.*) *$");
        foreach(QString line, cmd.lines){
            if(regex.exactMatch(line)){
                quint8 index = regex.cap(1).toUShort();
                if(_outputLabels.contains(index)){
                    _outputLabels[index] = regex.cap(2);
                }
                else{
                    _outputLabels.insert(index, regex.cap(2));
                }
            }
        }
        checkEndConfig();
        emit changed();
    }
    else if(cmd.command == "VIDEO OUTPUT ROUTING"){
        if(_blinking){
            return;
        }
        QRegExp regex("^([0-9]+) +([0-9]+) *$");
        foreach(QString line, cmd.lines){
            if(regex.exactMatch(line)){
                quint8 output = regex.cap(1).toUShort();
                if(_routingTable.contains(output)){
                    _routingTable[output] = regex.cap(2).toUShort();
                }
                else{
                    _routingTable.insert(output, regex.cap(2).toUShort());
                }
            }
        }
        checkEndConfig();
        emit changed();
    }
    else if(cmd.command == "VIDEOHUB DEVICE") { //hub description
        //nothing important for us
    }
    else if(cmd.command == "VIDEO OUTPUT LOCKS") { //lock status,used by ping
        //nothingto do, only used for ping cmd emulation
    }
    else {
        qDebug() << "Unknown command: " << cmd.command;
        foreach(QString line, cmd.lines){
            qDebug() << line;
        }
    }
}
