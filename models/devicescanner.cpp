#include "devicescanner.h"

#include <qfile.h>
#include <qsettings.h>
#include <qnetworkinterface.h>
#include <qprocess.h>

#include "../const.h"
#include "../utils/netutils.h"

DeviceScanner::DeviceScanner() :
    QObject(NULL), _udpSocket(this), _timer(this)
{
    QSettings settings("C:\\Users\\J\\Desktop\\sam\\winctrl.ini", QSettings::IniFormat);
#ifdef Q_OS_WIN32
    _defaultPaths.append("C:\\Program Files\\Nmap");
    _defaultPaths.append("C:\\Program Files (x86)\\Nmap");
#else
    _defaultPaths.append("/bin");
    _defaultPaths.append("/usr/bin");
    _defaultPaths.append("/usr/local/bin");
#endif
    connect(&_timer, SIGNAL(timeout()), this, SLOT(sayHello()));
}

DeviceScanner::~DeviceScanner() {
    foreach(QProcess *proc, _scanners){
        disconnect(proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(error(QProcess::ProcessError)));
        disconnect(proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));
        proc->close();
        proc->kill();
        delete(proc);
    }
}

void DeviceScanner::start() {
    _udpSocket.bind(CHAPI_BROADCAST_POST, QUdpSocket::ShareAddress);
    connect(&_udpSocket, SIGNAL(readyRead()), this, SLOT(helloReceived()));

    sayHello();

    _timer.start(2000);

    QSettings settings("C:\\Users\\J\\Desktop\\sam\\winctrl.ini", QSettings::IniFormat);
    if(checkNmap(settings.value("General/nmap_path", "").toString())){
        scan();
    }
    else {
        emit needNmap();
    }
}


void DeviceScanner::sayHello() {
    QByteArray datagram = "HELLO server 1.0\n";
    _udpSocket.writeDatagram(datagram.data(), datagram.size(), QHostAddress::Broadcast, CHAPI_BROADCAST_POST);
}

bool DeviceScanner::checkNmap(const QString &path) {
#ifdef Q_OS_WIN32
    QString exe = "/nmap.exe";
#else
    QString exe = "/nmap";
#endif
    foreach(QString defaultPath, _defaultPaths){
        QFile nmap(defaultPath + exe);
        if(nmap.exists()) {
            _nmapPath = defaultPath;
            return true;
        }
    }

    if(path == ""){
        return false;
    }
    else{
        QFile nmap(path + exe);
        return nmap.exists();
    }
}

void DeviceScanner::nmapPathDefined(QString path) {
    if(!checkNmap(path)){
        emit needNmap();
    }
    else {
        _nmapPath = path;
        QSettings settings("C:\\Users\\J\\Desktop\\sam\\winctrl.ini", QSettings::IniFormat);
        settings.setValue("General/nmap_path", path);
        scan();
    }
}

void DeviceScanner::scan() {
    QMap<quint32, QPair<QHostAddress, int> > detectedNetworks;
    foreach(QNetworkInterface inet, QNetworkInterface::allInterfaces()) {
        if(((inet.flags() & QNetworkInterface::IsLoopBack) == 0) && ((inet.flags() & QNetworkInterface::IsUp) == QNetworkInterface::IsUp)) {
            QHostAddress network = NetUtils::getNetwork(inet);
            if(!network.isNull()){
                int netMask = NetUtils::getMaskPrefix(inet);
                if(!_networks.contains(network.toIPv4Address())){
                    _networks.insert(network.toIPv4Address(), QPair<QHostAddress, int>(network, netMask));
                }
                detectedNetworks.insert(network.toIPv4Address(), QPair<QHostAddress, int>(network,netMask));
            }
        }
    }

    foreach(quint32 netIp, _networks.keys()){
        if(!detectedNetworks.contains(netIp)){
            //emit interfaceRemoved(_networks[netIp].first.toIPv4Address());
            _networks.remove(netIp);
            if(_scanners.contains(netIp)){
                disconnect(_scanners[netIp], SIGNAL(error(QProcess::ProcessError)), this, SLOT(error(QProcess::ProcessError)));
                disconnect(_scanners[netIp], SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));
                _scanners[netIp]->close();
                delete(_scanners[netIp]);
                _scanners.remove(netIp);
            }
        }
        else if(!_scanners.contains(netIp)){
            QProcess *scanner = new QProcess();
            QStringList args;
            args << "-oX" << "-" << (_networks[netIp].first.toString()+"/"+QString::number(_networks[netIp].second)) << "-p"
                 << STRINGIFY(CHAPI_TCP_PORT) "," STRINGIFY(CHAPISERVER_TCP_PORT) "," STRINGIFY(ATEM_PORT) "," STRINGIFY(VIDEOHUB_PORT);
#ifdef Q_OS_WIN32
            QString exe = "/nmap.exe";
#else
            QString exe = "/nmap";
#endif
            connect(scanner, SIGNAL(error(QProcess::ProcessError)), this, SLOT(error(QProcess::ProcessError)));
            connect(scanner, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));

            scanner->setWorkingDirectory(_nmapPath);
            scanner->start("\"" + _nmapPath + exe + "\"", args, QIODevice::ReadOnly);
            _scanners.insert(netIp, scanner);
        }
    }
}


void DeviceScanner::error(QProcess::ProcessError err){
    Q_UNUSED(err);
    QProcess *proc = (QProcess *)sender();
    cleanProc(proc);
}

void DeviceScanner::finished(int exitCode, QProcess::ExitStatus exitStatus){
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    QProcess *proc = (QProcess *)sender();

    QDomDocument doc;
    QString result = proc->readAllStandardOutput();
    doc.setContent(result);
    QDomElement root = doc.documentElement();
    QDomNodeList devList = root.elementsByTagName("host");
    int devListCount = devList.count();
    for(int i = 0; i < devListCount; i++) {
        parseScanResult(devList.at(i));
    }
    cleanProc(proc);
}

void DeviceScanner::cleanProc(QProcess *proc) {
    for (auto it = _scanners.begin(); it != _scanners.end();){
        if (it.value() == proc) {
            it = _scanners.erase(it);
        }
        else {
            ++it;
        }
    }
    disconnect(proc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(error(QProcess::ProcessError)));
    disconnect(proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(finished(int,QProcess::ExitStatus)));
    proc->close();
    delete(proc);

    if(_scanners.empty()){
        emit allScanFinished();
        QTimer::singleShot(1000, this, SLOT(scan()));
    }
}

void DeviceScanner::parseScanResult(const QDomNode &devNode) {
    QDomNodeList devInfo = devNode.toElement().elementsByTagName("address");

    QString name = "";
    QString ip = "";
    quint64 mac = 0;
    QString status = "unreachable";
    QString macStr = "";
    bool vhPortOpened = false;
    bool atemPortOpened = false;
    bool cfgPortOpened = false;
    bool serverPortOpened = false;

    QDomNode childNode = devNode.firstChild();
    while (!childNode.isNull()) {
        QDomElement child = childNode.toElement();
        if (child.tagName() == "status") {
            status = child.attribute("state", "down");
        }
        else if (child.tagName() == "address") {
            if(child.attribute("addrtype") == "ipv4"){
                ip = child.attribute("addr");
            }
            else if(child.attribute("addrtype") == "mac"){
                macStr = child.attribute("addr");
                mac = NetUtils::strToMac(child.attribute("addr"));
                if(name == "" && child.hasAttribute("vendor")){
                    name = child.attribute("vendor", "");
                }
            }
        }
        else if(child.tagName() == "hostnames" && !child.firstChild().isNull()){
            name = child.firstChild().toElement().attribute("name", name);
        }
        else if(child.tagName() == "ports"){
            QDomNodeList ports = child.elementsByTagName("port");
            int nbr = ports.count();
            for(int i = 0; i < nbr; i++){
                QDomElement port = ports.at(i).toElement();
                if(port.hasAttribute("protocol") && (port.attribute("protocol") == "udp") &&
                        port.hasAttribute("portid") && port.attribute("portid") == STRINGIFY(ATEM_PORT) && port.elementsByTagName("state").count() > 0){
                    QDomElement state = port.elementsByTagName("state").at(0).toElement();
                    if(state.hasAttribute("state") && state.attribute("state") ==  "open"){
                        atemPortOpened = true;
                    }
                }
                if(port.hasAttribute("protocol") && (port.attribute("protocol") == "tcp") &&
                        port.hasAttribute("portid") && port.attribute("portid") == STRINGIFY(VIDEOHUB_PORT) && port.elementsByTagName("state").count() > 0){
                    QDomElement state = port.elementsByTagName("state").at(0).toElement();
                    if(state.hasAttribute("state") && state.attribute("state") ==  "open"){
                        vhPortOpened = true;
                    }
                }
                if(port.hasAttribute("protocol") && (port.attribute("protocol") == "tcp") &&
                        port.hasAttribute("portid") && port.attribute("portid") == STRINGIFY(CHAPI_TCP_PORT) && port.elementsByTagName("state").count() > 0){
                    QDomElement state = port.elementsByTagName("state").at(0).toElement();
                    if(state.hasAttribute("state") && state.attribute("state") ==  "open"){
                        cfgPortOpened = true;
                    }
                }
                if(port.hasAttribute("protocol") && (port.attribute("protocol") == "tcp") &&
                        port.hasAttribute("portid") && port.attribute("portid") == STRINGIFY(CHAPISERVER_TCP_PORT) && port.elementsByTagName("state").count() > 0){
                    QDomElement state = port.elementsByTagName("state").at(0).toElement();
                    if(state.hasAttribute("state") && state.attribute("state") ==  "open"){
                        cfgPortOpened = true;
                    }
                }
            }
        }
        childNode = child.nextSibling();
    }

    if(mac != 0){ // ignore local interface
        emit deviceDetected(mac, ip, name, guessType(cfgPortOpened, vhPortOpened, atemPortOpened, serverPortOpened, ip));
    }
}

Device::DeviceType DeviceScanner::guessType(bool cfgPort, bool vhPort, bool atemPort, bool serverPort, const QString &ip) {
    if(serverPort){
        return Device::ChapiServer;
    }
    if(cfgPort){
        return Device::ChapiDev;
    }
    if(vhPort){
        return Device::VideoHub;
    }
    if(atemPort) {
        return Device::Atem;
    }
    else{
        foreach(QHostAddress gateway, NetUtils::getGateways()) {
            if(gateway.toString() == ip){
                return Device::Router;
            }
        }
        return Device::UnknownDevice;
    }
}

void DeviceScanner::helloReceived() {
    while (_udpSocket.hasPendingDatagrams()) {
        QByteArray data;
        qint64 dataSize = _udpSocket.pendingDatagramSize();
        data.fill(0, dataSize+1);
        _udpSocket.readDatagram(data.data(), dataSize);

        QString msg = QString::fromLatin1(data);
        QString macStr;
        QString ip;

        QRegExp regex("^HELLO chapi [0-9]+\\.[0-9]+ ((?:[A-fa-f0-9]{2}:){5}[A-fa-f0-9]{2}) ((?:[0-9]{1,3}\\.){3}[0-9]{1,3})\n$");
        if(regex.exactMatch(msg)){
            macStr = regex.cap(1);
            ip = regex.cap(2);
            emit deviceDetected(NetUtils::strToMac(macStr), ip, "", Device::ChapiDev);
        }
    }
}
