#include "acesscontrolmanager.h"
#include <QDBusConnection>
#include <QDBusVariant>
#include <QProcess>
#include <QDebug>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "ddiskmanager.h"
#include "dblockdevice.h"

#include "app/policykithelper.h"
#include "dbusservice/dbusadaptor/acesscontrol_adaptor.h"

QString AcessControlManager::ObjectPath = "/com/deepin/filemanager/daemon/AcessControlManager";
QString AcessControlManager::PolicyKitActionId = "com.deepin.filemanager.daemon.AcessControlManager";

AcessControlManager::AcessControlManager(QObject *parent)
    : QObject(parent)
    , QDBusContext()
{
    qDebug() << "register:" << ObjectPath;
    if (!QDBusConnection::systemBus().registerObject(ObjectPath, this)) {
        qFatal("=======AcessControlManager Register Object Failed.");
    }
    m_acessControlAdaptor = new AcessControlAdaptor(this);
    m_diskMnanager = new DDiskManager(this);
    m_diskMnanager->setWatchChanges(true);
    qDebug() << "=======AcessControlManager() ";

    initConnect();

}

AcessControlManager::~AcessControlManager()
{
    qDebug() << "~AcessControlManager()";
}

void AcessControlManager::initConnect()
{
    qDebug() << "AcessControlManager::initConnect()";
    connect(m_diskMnanager, &DDiskManager::mountAdded, this, &AcessControlManager::chmodMountpoints);
}

bool AcessControlManager::checkAuthentication()
{
    bool ret = false;
    qint64 pid = 0;
    QDBusConnection c = QDBusConnection::connectToBus(QDBusConnection::SystemBus, "org.freedesktop.DBus");
    if(c.isConnected()) {
       pid = c.interface()->servicePid(message().service()).value();
    }

    if (pid){
        ret = PolicyKitHelper::instance()->checkAuthorization(PolicyKitActionId, pid);
    }

    if (!ret) {
        qDebug() << "Authentication failed !!";
        qDebug() << "failed pid: " << pid;
        qDebug() << "failed policy id:" << PolicyKitActionId;
    }
    return ret;
}

// 废弃
bool AcessControlManager::acquireFullAuthentication(const QString &userName, const QString &path)
{
    Q_UNUSED(userName)

    bool ret = false;
    QByteArray pathBytes(path.toLocal8Bit());
    struct stat fileStat;
    stat(pathBytes.data(), &fileStat);
    if (chmod(pathBytes.data(), (fileStat.st_mode | S_IWUSR | S_IWGRP | S_IWOTH)) == 0) {
        qDebug() << "chmod() success!";
        ret = true;
    }
    return ret;
}

void AcessControlManager::chmodMountpoints()
{
    for (const QString &dev : DDiskManager::blockDevices({})) {
        QScopedPointer<DBlockDevice> blk(DDiskManager::createBlockDevice(dev));
        for(auto &mp : blk->mountPoints()) {
            struct stat fileStat;
            qDebug() << "chmod ==> " << mp;
            stat(mp.data(), &fileStat);
            chmod(mp.data(), (fileStat.st_mode | S_IWUSR | S_IWGRP | S_IWOTH));
        }
    }
#if 0
    // system call
    struct mntent *ent = NULL;
    FILE *aFile = NULL;

    aFile = setmntent("/proc/mounts", "r");
    if (aFile == NULL) {
      perror("setmntent()");
      return;
    }
    while (NULL != (ent = getmntent(aFile))) {
        QString fsName(ent->mnt_fsname);
        QString mntDir(ent->mnt_dir);
        if (fsName.startsWith("/")) {
            qDebug() << "mount fs name: " << fsName << ", mount path:" << mntDir;
            struct stat fileStat;
            stat(ent->mnt_dir, &fileStat);
            if (chmod(ent->mnt_dir, (fileStat.st_mode | S_IWUSR | S_IWGRP | S_IWOTH)) == 0) {
                qDebug() << "chmod " << mntDir << "success";
            } else {
                qDebug() << "chmod " << mntDir << "faild";
            }
        }
    }
    endmntent(aFile);
#endif
}