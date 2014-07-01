#include "chapiview.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QInputDialog>
#include <QKeyEvent>
#include <QDialogButtonBox>

#include "networksettingsview.h"
#include "../utils/qclickablelabel.h"
#include "../models/device.h"
#include "../models/chapidevice.h"

ChapiView::ChapiView(ChapiDevice *dev, DeviceList *devList, QWidget *parent) :
    QIntegratedFrame(parent)
{
    _dev = dev;
    _devList = devList;
    _netCfg = dev->networkConfig();

    QVBoxLayout *layout = new QVBoxLayout();
    setLayout(layout);

    setMaximumWidth(450);

    QHBoxLayout *labelBox = new QHBoxLayout();
    labelBox->addStretch(1);

    QIcon icon;
    icon.addFile(QStringLiteral(":/icons/chapi.png"), QSize(), QIcon::Normal, QIcon::Off);
    QClickableLabel *clickableLabel = new QClickableLabel();
    connect(clickableLabel, SIGNAL(doubleClick()), this, SLOT(onNameDoubleClick()));
    clickableLabel->setPixmap(icon.pixmap(32, 32));
    labelBox->addWidget(clickableLabel);

    _nameLabel = new QClickableLabel();
    connect(_nameLabel, SIGNAL(doubleClick()), this, SLOT(onNameDoubleClick()));
    QFont font(_nameLabel->font());
    font.setBold(true);
    _nameLabel->setFont(font);
    _nameLabel->setText(_dev->name());
    labelBox->addWidget(_nameLabel);

    labelBox->addStretch(1);
    layout->addLayout(labelBox);
    layout->addStretch(2);

    QIcon icon2;
    icon2.addFile(QStringLiteral(":/icons/network.png"), QSize(), QIcon::Normal, QIcon::Off);
    QPushButton *networkButton = new QPushButton(icon2, tr("réseau"));
    networkButton->setIconSize(QSize(24, 24));

    layout->addWidget(networkButton);
    connect(networkButton, SIGNAL(clicked()), this, SLOT(onNetworkBtnClicked()));

    layout->addStretch(2);

    QDialogButtonBox *btnBox = new QDialogButtonBox();
    QPushButton *restartBtn = btnBox->addButton("Redémarer", QDialogButtonBox::ActionRole);
    connect(restartBtn, SIGNAL(clicked()), this, SLOT(onRestartClicked()));
    QPushButton *resetBtn = btnBox->addButton("Reset", QDialogButtonBox::ActionRole);
    connect(resetBtn, SIGNAL(clicked()), this, SLOT(onResetClicked()));
    QPushButton *cancelBtn = btnBox->addButton("Annuler", QDialogButtonBox::RejectRole);
    connect(cancelBtn, SIGNAL(clicked()), this, SIGNAL(exitDeviceSettings()));
    QPushButton *okBtn = btnBox->addButton("Ok", QDialogButtonBox::ApplyRole);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkClicked()));
    layout->addWidget(btnBox);
    setFocus();
    layout->setSizeConstraint(QLayout::SetFixedSize);
}

void ChapiView::onNetworkBtnClicked() {
    NetworkSettingsView *net = new NetworkSettingsView(&_netCfg, _dev, _devList, this);
    net->show();
}

void ChapiView::onNameDoubleClick() {
    bool ok = true;
    QString text = QInputDialog::getText(this, tr("Renommer"),
                tr("Nouveau Nom:"), QLineEdit::Normal, _nameLabel->text(), &ok);
    if(ok && !text.isEmpty()) {
        _nameLabel->setText(text);
        _newName = text;
    }
}

void ChapiView::onResetClicked() {
    _dev->reset();
    emit exitDeviceSettings();
}

void ChapiView::onRestartClicked() {
    _dev->restart();
    emit exitDeviceSettings();
}

void ChapiView::onOkClicked() {
    if(!_newName.isEmpty()){
        _dev->setName(_newName);
    }
    emit exitDeviceSettings();
}
