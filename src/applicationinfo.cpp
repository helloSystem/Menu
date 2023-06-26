#include "applicationinfo.h"
#include <KWindowInfo>
#include <QDebug>
#include <QStringList>
#include <QString>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>

#include <limits.h>

#include <sys/socket.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/user.h>
#include <QDir>
#include <fcntl.h>
#if defined(__FreeBSD__)
#  include <sys/sysctl.h>
#  include <libprocstat.h>
#endif
#if defined(__linux__)
#  include <proc/readproc.h>
#endif
#include <QX11Info>

// Theory of operation:
//
// For a functional desktop, we need to know things
// about windows, processes, files, and directories.
// The general idea is that we store such information
// directly on those objects themselves - not in some
// other place. This concept greatly simplifies everything!
//
// Store information that is interesting about a window
// directly on the window itself, so that it can be easily
// retrieved from there quickly by reading an atom.
// For example, if we are interested in an environment variable
// of the process that has created the window, then we
// need to find out the process ID of the process,
// need to find out the file path of the executable that has created the process,
// get its environment variables, search for the environment variable
// we are interested in, and extract its value.
// Once we have it, we store this information
// directly on the window itself, so that it can be easily
// retrieved next time we need it. And fast!
//
// Similarly for information that is intreresting about files and directories,
// in which case we may do a similar thing using extended attributes.

ApplicationInfo::ApplicationInfo() { }

ApplicationInfo::~ApplicationInfo() { }

// Returns the name of the most nested bundle a file is in,
// or an empty string if the file is not in a bundle
QString ApplicationInfo::bundlePath(QString path)
{
    QDir(path).cleanPath(path);
    // Remove trailing slashes
    while (path.endsWith("/")) {
        path.remove(path.length() - 1, 1);
    }
    if (path.endsWith(".app")) {
        return path;
    } else if (path.contains(".app/")) {
        QStringList parts = path.split(".app");
        parts.removeLast();
        return parts.join(".app");
    } else if (path.endsWith(".AppDir")) {
        return path;
    } else if (path.contains(".AppDir/")) {
        QStringList parts = path.split(".AppDir");
        parts.removeLast();
        return parts.join(".AppDir");
    } else if (path.endsWith(".AppImage")) {
        return path;
    } else {
        return "";
    }
}

QString ApplicationInfo::applicationNiceNameForPath(QString path)
{
    QString applicationNiceName;
    QString bp = bundlePath(path);
    if (bp != "") {
        applicationNiceName = QFileInfo(bp).completeBaseName();
    } else {
        applicationNiceName =
                QFileInfo(path).fileName(); // TODO: Somehow figure out via the desktop file a
                                            // properly capitalized name...
    }
    return applicationNiceName;
}

QString ApplicationInfo::environmentVariableForPId(unsigned int pid,
                                                   const QByteArray &environmentVariable)
{
    QString path;

#if defined(__FreeBSD__)

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
        const QString &entry = QString::fromLocal8Bit(envs[i]);
        const int splitPos = entry.indexOf('=');

        if (splitPos != -1) {
            const QString &name = entry.mid(0, splitPos);
            const QString &value = entry.mid(splitPos + 1, -1);
            // qDebug() << "name:" << name;
            // qDebug() << "value:" << value;
            if (name == environmentVariable) {
                path = value;
                break;
            }
        }
    }

    procstat_freeenvv(prstat);
    procstat_close(prstat);

#elif defined(__linux__)
    pid_t pidList[1];
    pidList[0] = pid;
    PROCTAB *processes = openproc(PROC_PID | PROC_FILLENV, pidList, 1);
    proc_t *proc_inf;
    proc_inf = readproc(processes, NULL);
    while (proc_inf) {
        if (proc_inf->environ) {
            char *envvar = *proc_inf->environ;
            while (envvar) {
                char *key = strtok(envvar, "=");
                if (key != NULL && strncmp(environmentVariable.toStdString().c_str(), key,
                            strlen(environmentVariable))
                    == 0) {
                    path = QString::fromLocal8Bit(strtok(NULL, "="));
                    if (!path.isEmpty())
                        return path;
                }
                proc_inf->environ++;
                envvar = *(proc_inf->environ);
            }
        }
        proc_inf = readproc(processes, NULL);
    }
    path = "";
#else
    path = "NotImplemeted";
#endif

    return path;
}

// Returns the name of the bundle
// based on the LAUNCHED_BUNDLE environment variable set by the 'launch' command
QString ApplicationInfo::bundlePathForPId(unsigned int pid)
{
    return environmentVariableForPId(pid, "LAUNCHED_BUNDLE");
}

const QString ApplicationInfo::environmentVariableForWId(unsigned long long winId,
                                                         const QByteArray &environmentVariable)
{

    const QByteArray key = "_ENV_LAUNCHED_BUNDLE";
    const QString existingValue = getWindowProperty(winId, key);

    if (existingValue != nullptr) {
        // If window has such an atom, return its value
        // qDebug() << winId << ": Window has" << key << "atom, using its value:" << existingValue;
        return existingValue;
    } else {
        // Set atom on the window and return its value
        KWindowInfo info(winId, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);
        const unsigned int pid = info.pid();
        auto value = environmentVariableForPId(pid, environmentVariable);
        if (value == nullptr) {
            setWindowProperty(
                    winId, key,
                    nullptr); // So that the atom will be set, even if it it set to an empty value
            qDebug() << winId << ": Populated" << key << "atom with blank value";
            return nullptr;
        }
        setWindowProperty(winId, key, value.toUtf8());
        qDebug() << winId << ": Populated" << key << "atom from process environment:" << value;
        return value;
    }
}

const QString ApplicationInfo::bundlePathForWId(unsigned long long winId)
{

    return (environmentVariableForWId(winId, "LAUNCHED_BUNDLE"));
}

QString ApplicationInfo::pathForWId(unsigned long long winId)
{

    const QByteArray key = "_EXECUTABLE_PATH";
    const QString existingValue = getWindowProperty(winId, key);

    if (existingValue != nullptr) {
        // If window has such an atom, return its value
        // qDebug() << winId << ": Window has" << key << "atom, using its value:" << existingValue;
        return existingValue;
    } else {
        // Set atom on the window and return its value
        QString path;
        KWindowInfo info(winId, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);

        // qDebug() << "probono: info.pid():" << info.pid();
        // qDebug() << "probono: info.windowClassName():" << info.windowClassName();
#if defined(__FreeBSD__)
        struct procstat *prstat = procstat_open_sysctl();
        if (prstat == NULL) {
            return "";
        }
        unsigned int cnt;
        kinfo_proc *procinfo = procstat_getprocs(prstat, KERN_PROC_PID, info.pid(), &cnt);
        if (procinfo == NULL || cnt != 1) {
            procstat_close(prstat);
            return "";
        }
        char **pargs = procstat_getargv(prstat, procinfo, 0);
        if (pargs == NULL) {
            procstat_close(prstat);
            return "";
        }
        path = QString::fromLocal8Bit(pargs[0]);
        procstat_close(prstat);
        procstat_freeargv(prstat);

#endif
#if defined(__linux__)
        pid_t pidList[1];
        pidList[0] = info.pid();
        qDebug() << info.pid();
        PROCTAB *processes = openproc(PROC_PID | PROC_FILLCOM, pidList, 1);
        proc_t *proc_inf;
        proc_inf = readproc(processes, NULL);
        while (proc_inf) {
            if (proc_inf->cmdline) {

                path = QString::fromLocal8Bit(*proc_inf->cmdline);
                return path;
            }
            proc_inf = readproc(processes, NULL);
        }

#endif

        qDebug() << "probono: pathForWId returns:" << path;

        setWindowProperty(winId, key, path.toUtf8());
        qDebug() << winId << ": Populated" << key << "atom from process environment:" << path;

        return path;
    }
    return "unknown";
}

QString ApplicationInfo::applicationNiceNameForWId(unsigned long long winId)
{
    KWindowInfo info(winId, NET::WMPid, NET::WM2TransientFor | NET::WM2WindowClass);
    QString applicationNiceName = applicationNiceNameForPath(bundlePathForPId(info.pid()));
    if (applicationNiceName.isEmpty()) {
        applicationNiceName = QFileInfo(pathForWId(winId)).fileName();
    }
    return applicationNiceName;
}

// Based on appmenu/appmenumodel.cpp
// https://en.wikipedia.org/wiki/Rule_of_three_(computer_programming)
const QString ApplicationInfo::getWindowProperty(WId winId, const QByteArray &name)
{
    QByteArray value;

    auto *c = QX11Info::connection();

    xcb_atom_t atom;
    QHash<QByteArray, xcb_atom_t> s_atoms;

    if (!s_atoms.contains(name)) {
        const xcb_intern_atom_cookie_t atomCookie =
                xcb_intern_atom(c, false, name.length(), name.constData());
        QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> atomReply(
                xcb_intern_atom_reply(c, atomCookie, nullptr));

        if (atomReply.isNull()) {
            return value;
        }

        s_atoms[name] = atomReply->atom;

        if (s_atoms[name] == XCB_ATOM_NONE) {
            return nullptr; // the atom does not yet exist
        }
    }

    static const long MAX_PROP_SIZE = 10000;
    auto propertyCookie =
            xcb_get_property(c, false, winId, s_atoms[name], XCB_ATOM_STRING, 0, MAX_PROP_SIZE);
    QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> propertyReply(
            xcb_get_property_reply(c, propertyCookie, nullptr));

    if (propertyReply.isNull()) {
        return QString("");
    }

    if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8
        && propertyReply->value_len > 0) {
        const char *data = (const char *)xcb_get_property_value(propertyReply.data());
        int len = propertyReply->value_len;

        if (data) {
            value = QByteArray(data, data[len - 1] ? len : len - 1);
        }
    }

    return QString(value);
};

// Based on QtPlugin/platformthemeplugin/x11integration.cpp
// https://en.wikipedia.org/wiki/Rule_of_three_(computer_programming)
void ApplicationInfo::setWindowProperty(WId winId, const QByteArray name, const QByteArray value)
{
    auto *c = QX11Info::connection();

    xcb_atom_t atom;
    QHash<QByteArray, xcb_atom_t> m_atoms;

    auto it = m_atoms.find(name);
    if (it == m_atoms.end()) {
        const xcb_intern_atom_cookie_t cookie =
                xcb_intern_atom(c, false, name.length(), name.constData());
        QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> reply(
                xcb_intern_atom_reply(c, cookie, nullptr));
        if (!reply.isNull()) {
            atom = reply->atom;
            m_atoms[name] = atom;
        } else {
            return;
        }
    } else {
        atom = *it;
    }
    // If we wanted to delete the atom instead of emptying it, we could do: xcb_delete_property(c,
    // winId, atom);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, winId, atom, XCB_ATOM_STRING, 8, value.length(),
                        value.constData());
}
