#include "chapidevice.h"

#include "../const.h"
#include "../utils/netutils.h"

ChapiDevice::ChapiDevice(const Device &dev) : ConnectedDevice(dev) {
    _netCfg.useDHCP = true;
}

ChapiDevice::~ChapiDevice(){
}

quint16 ChapiDevice::port() const {
    return CHAPI_TCP_PORT;
}

int ChapiDevice::pingDelay() const {
    return 500;
}

quint32 ChapiDevice::pingLostTolerance() const {
    return 2;
}

int ChapiDevice::reconnectDelay() const {
    return 2000;
}

bool ChapiDevice::isConfigurable() const {
    return true;
}

bool ChapiDevice::isIdentifiable() const {
    return true;
}

Device::DeviceSimpeStatus ChapiDevice::simpleStatus() const {
    if(_status == Device::Ready){
        return Device::Green;
    }
    if((_status == Device::Unreachable) || (_status == Device::Located)) {
        return Device::Red;
    }
    return Device::Yellow;
}

void ChapiDevice::blink() {
    _socket.write("BLINK\n");
    _socket.flush();
}

void ChapiDevice::reset() {
    _socket.write("RESET\n");
    _socket.flush();
    emit resetIp();
    closeCnx(false);
}

void ChapiDevice::restart() {
    _socket.write("RESTART\n");
    _socket.flush();
    emit resetIp();
    closeCnx(false);
}

void ChapiDevice::parseInput(){
    while(_socket.canReadLine()){
        QByteArray buffer = _socket.readLine();
        QString line(buffer);
        parseLine(line.trimmed());
    }
}


NetworkConfig ChapiDevice::networkConfig(){
    return _netCfg;
}

void ChapiDevice::ping() {
    _socket.write("PING\n");
    _socket.flush();
}

void ChapiDevice::updateDeviceStatus(){
    if(!_configSet) {
        setStatus(Device::Unconfigured);
    }
    else {
        switch(_cnxStatus){
            case ChapiDevice::Connected:
                setStatus(Device::Ready);
                break;

            case ChapiDevice::Connecting:
                setStatus(Device::ConnectingToHub);
                break;

            default:
            case ChapiDevice::Unreachable:
                setStatus(Device::HubUnreachable);
                break;
        }
    }
}

const char configSetStr[] = "CONFIG_SET:";
const char nbrBtnsStr[] = "NBR_BUTTONS:";
const char dhcpStr[] = "DHCP:";
const char ipStr[] = "IP:";
const char netmaskStr[] = "NETMASK:";
const char gatewayStr[] = "GATEWAY:";
const char targetMacStr[] = "TARGET_MAC:";
const char ouputStr[] = "MEM_OUTPUT:";
const char inputStr[] = "MEM_INPUT:";
const char cnxStatusStr[] = "CNX_STATUS:";

void ChapiDevice::parseLine(const QString &line){
    if(line == "PONG"){
        _pingSent = 0;
    }
    else if(line == "CONFIG_BEGIN"){
        setStatus(Device::ReadingConfig);
    }
    else if(line == "CONFIG_END") {
        updateDeviceStatus();
    }
    else if(line.startsWith(configSetStr)){
        qDebug() << line;
        QString result = line.right(line.length() - sizeof(configSetStr)).trimmed();
        bool ok;
        int value = result.toInt(&ok);
        _configSet = ok && (value == 1);
    }
    else if(line.startsWith(nbrBtnsStr)){
        qDebug() << line;
        QString result = line.right(line.length() - sizeof(nbrBtnsStr)).trimmed();
        bool ok;
        int value = result.toInt(&ok);
        if(ok){
            _nbrBtns = value;
        }
    }
    else if(line.startsWith(dhcpStr)){
        qDebug() << line;
        QString result = line.right(line.length() - sizeof(dhcpStr)).trimmed();
        bool ok;
        int value = result.toInt(&ok);
        _netCfg.useDHCP = ok && (value == 1);
    }
    else if(line.startsWith(ipStr)){
        _netCfg.ip = line.right(line.length() - sizeof(ipStr)).trimmed();
    }
    else if(line.startsWith(netmaskStr)){
        _netCfg.netmask = line.right(line.length() - sizeof(netmaskStr)).trimmed();
    }
    else if(line.startsWith(gatewayStr)){
        _netCfg.gateway = line.right(line.length() - sizeof(gatewayStr)).trimmed();
    }
    else if(line.startsWith(targetMacStr)){
        _targetMac = NetUtils::strToMac(line.right(line.length() - sizeof(targetMacStr)).trimmed());
    }
    else if(line.startsWith(ouputStr)){
        qDebug() << line;
        QString result = line.right(line.length() - sizeof(ouputStr)).trimmed();
        bool ok;
        int value = result.toInt(&ok);
        if(ok){
            _outputIndex = value;
        }
    }
    else if(line.startsWith(inputStr)){
        QStringList result = line.right(line.length() - sizeof(inputStr)).trimmed().split(' ', QString::SkipEmptyParts);
        if(result.length() == 2){
            bool okIndex;
            int index = result[0].toInt(&okIndex);
            bool okDest;
            int dest = result[1].toInt(&okDest);
            if(okIndex && okDest){
                _inputIndexes.insert(index, dest);
            }
        }
    }
    else if(line.startsWith(cnxStatusStr)){
        QString result = line.right(line.length() - sizeof(cnxStatusStr)).trimmed();
        bool ok;
        int value = result.toInt(&ok);
        if(ok && (value > 0) && (value < 4)){
            _cnxStatus = (ChapiDevice::CnxStatus)value;
            updateDeviceStatus();
        }
    }
    else {
        qDebug() << line;
    }
}
