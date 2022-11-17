#include "applicationwindow.h"
#include <KWindowInfo>
#include <QDebug>
#include <QStringList>
#include <QString>
#include <QProcess>
#include <QFile>
#include <QFileInfo>

#include <sys/syslimits.h>
#if defined(__FreeBSD__)
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>
#include <QDir>
#include <fcntl.h>
#include <libprocstat.h>
#endif

// Returns the name of the most nested bundle a file is in,
// or an empty string if the file is not in a bundle
QString bundlePath(QString path) {
    QDir(path).cleanPath(path);
    // Remove trailing slashes
    while( path.endsWith("/") ){
        path.remove(path.length()-1,1);
    }
    if (path.endsWith(".app")) {
        return path;
    } else if (path.contains(".app/")) {
        QStringList parts = path.split(".app");
        parts.removeLast();
        return parts.join(".app");
    } else if (path.endsWith(".AppDir")) {
        return path;
    } else if ( path.contains(".AppDir/")) {
        QStringList parts = path.split(".AppDir");
        parts.removeLast();
        return parts.join(".AppDir");
    } else if (path.endsWith(".AppImage")) {
        return path;
    } else {
        return "";
    }

}

// Returns the name of the bundle
QString bundleName(unsigned long long id) {
    return "";
}

QString applicationNiceNameForPath(QString path) {
    QString applicationNiceName;
    QString bp = bundlePath(path);
    if (bp != "") {
        applicationNiceName = QFileInfo(bp).completeBaseName();
    } else {
        applicationNiceName = QFileInfo(path).fileName(); // TODO: Somehow figure out via the desktop file a properly capitalized name...
    }
    return applicationNiceName;
}

// Returns the name of the bundle
// based on the LAUNCHED_BUNDLE environment variable set by the 'launch' command
QString bundlePathForPId(unsigned int pid) {
    QString path;

    if (QFile::exists("/usr/bin/procstat")) {
        // FreeBSD

        struct procstat *prstat = procstat_open_sysctl();
        if (prstat == NULL) {
            return "";
        }
        unsigned int cnt;
        kinfo_proc *procinfo = procstat_getprocs(prstat, KERN_PROC_PID, pid, &cnt);
        if (procinfo == NULL || cnt != 1) {
            procstat_close(prstat);
            return "";
        }
        char **envs = procstat_getenvv(prstat, procinfo, 0);
        if (envs == NULL) {
            procstat_close(prstat);
            return "";
        }

        for (int i = 0; envs[i] != NULL; i++) {
            const QString& entry = QString::fromLocal8Bit(envs[i]);
            const int splitPos = entry.indexOf('=');

            if (splitPos != -1) {
                const QString& name = entry.mid(0, splitPos);
                const QString& value = entry.mid(splitPos + 1, -1);
                // qDebug() << "name:" << name;
                // qDebug() << "value:" << value;
                if(name == "LAUNCHED_BUNDLE") {
                    path = value;
                    break;
                }
            }
        }

        procstat_freeenvv(prstat);
        procstat_close(prstat);

    } else if (QFile::exists(QString("/proc/%1/environ").arg(pid))) {
        // Linux
        qDebug() << "probono: TODO: Implement getting env";
        path = "ThisIsOnlyImplementedForFreeBSDSoFar";
    }

    // qDebug() << "probono: bundlePathForPId returns:" << path;
    return path;
}

QString bundlePathForWId(unsigned long long id) {
    QString path;
    KWindowInfo info(id, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);
    return bundlePathForPId(info.pid());
}

QString pathForWId(unsigned long long id) {
    QString path;
    KWindowInfo info(id, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);

    // qDebug() << "probono: info.pid():" << info.pid();
    // qDebug() << "probono: info.windowClassName():" << info.windowClassName();

    QProcess p;
    QStringList arguments;
    if (QFile::exists(QString("/proc/%1/file").arg(info.pid()))) {
        // FreeBSD
        arguments = QStringList() << "-f" << QString("/proc/%1/file").arg(info.pid());
    } else if (QFile::exists(QString("/proc/%1/exe").arg(info.pid()))) {
        // Linux
        arguments = QStringList() << "-f" << QString("/proc/%1/exe").arg(info.pid());
    }
    p.start("readlink", arguments);
    p.waitForFinished();
    QString retStr(p.readAllStandardOutput().trimmed());
    if(! retStr.isEmpty()) {
        // qDebug() << "probono:" << p.program() << p.arguments();
        // qDebug() << "probono: retStr:" << retStr;
        path = retStr;
    }
    // qDebug() << "probono: pathForWId returns:" << path;
    return path;
}

QString applicationNiceNameForWId(unsigned long long id) {
    QString path;
    QString applicationNiceName;
    KWindowInfo info(id, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);
    applicationNiceName = applicationNiceNameForPath(bundlePathForPId(info.pid()));
    if(applicationNiceName.isEmpty()) {
        applicationNiceName = QFileInfo(pathForWId(id)).fileName();
    }
    return applicationNiceName;
}
