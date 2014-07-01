#ifndef CHAPIVIEW_H
#define CHAPIVIEW_H

#include "../utils/qintegratedframe.h"
#include "../models/networkconfig.h"


class ChapiDevice;
class DeviceList;
class QClickableLabel;
class QChildEvent;

class ChapiView : public QIntegratedFrame
{
    Q_OBJECT
public:
    explicit ChapiView(ChapiDevice *dev, DeviceList *devList, QWidget *parent = 0);

private:
    ChapiDevice *_dev;
    DeviceList *_devList;
    NetworkConfig _netCfg;

    QClickableLabel *_nameLabel;

    QString _newName;

public slots:
    void onNetworkBtnClicked();
    void onNameDoubleClick();
    void onResetClicked();
    void onRestartClicked();
    void onOkClicked();
};

#endif // CHAPIVIEW_H
