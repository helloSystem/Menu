/*
 * Copyright (C) 2020 PandaOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include <QApplication>
#include <QDebug>
#include <QtDBus/QtDBus>
#include <csignal>

#include <qtsingleapplication/qtsingleapplication.h>

#include <QLibraryInfo>

// our main window
MainWindow *window;
// probono: Subclassing QApplication so that we can see what events are going on
// https://stackoverflow.com/a/27607947
class Application final : public QApplication
{
public:
    Application(int &argc, char **argv) : QApplication(argc, argv) { }
    virtual bool notify(QObject *receiver, QEvent *e) override
    {
        // your code here
        qDebug() << "probono: e.type():" << e->type();
        // qDebug() << "probono: QApplication::focusWidget():" << QApplication::focusWidget();
        if (QApplication::focusWidget() != nullptr) {
            qDebug() << "probono: QApplication::focusWidget()->objectName():"
                     << QApplication::focusWidget()->objectName();
            qDebug() << "probono: QApplication::focusWidget()->metaObject()->className():"
                     << QApplication::focusWidget()->metaObject()->className();
        }
        return QApplication::notify(receiver, e);
    }
};

/*
// Rebuild the system menu on SIGUSR1
// https://github.com/helloSystem/Menu/issues/16
void rebuildSystemMenuSignalHandler(int sig)
{
    window->m_MainWidget->rebuildSystemMenu();
}
*/

// probono: Using QtSingleApplication so that only one instance can run at any time,
// launching it again just brings the running instance into focus
// https://github.com/qtprojectqtproject/qt-solutions/blob/master/qtsingleapplication/examples/trivial/main.cpp

int main(int argc, char **argv)
{
    QtSingleApplication instance(argc, argv, true);
    if (instance.sendMessage("Wake up!")) {
        return 0;
    }

    // // probono: Use this instead of the next line for debugging
    // Application *a = new Application(argc, argv); // probono: Use this instead of the line above
    // for production

    QTranslator *qtTranslator = new QTranslator(&instance);
    QTranslator *translator = new QTranslator(&instance);

    // Install the translations built-into Qt itself
    qDebug() << "probono: QLocale::system().name()" << QLocale::system().name();
    if (!qtTranslator->load("qt_" + QLocale::system().name(),
                            QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
        // Other than qDebug, qCritical also works in Release builds
        qCritical() << "Failed qtTranslator->load";
    } else {
        if (!qApp->installTranslator(qtTranslator)) {
            qCritical() << "Failed qApp->installTranslator(qtTranslator)";
        }
    }

    // Install our own translations
    if (!translator->load("menubar_" + QLocale::system().name(),
                          QCoreApplication::applicationDirPath()
                                  + QString("/Resources/translations/"))) { // probono: .app bundle
        qDebug() << "probono: loading translations from .app bundle not successful";
        if (!translator->load(
                    "menubar_" + QLocale::system().name(),
                    QCoreApplication::applicationDirPath())) { // probono: When qm files are next to
                                                               // the executable ("uninstalled"),
                                                               // useful during development
            qCritical() << "Failed translator->load";
        }
    }

    if (!translator->isEmpty()) {
        if (!qApp->installTranslator(translator)) {
            qCritical() << "Failed qApp->installTranslator(translator)";
        }
    }

    MainWindow w;
    window = &w;
    // QTimer::singleShot(500, window, &MainWindow::show); // probono: Will this prevent the menu
    // from showing up in random places for a slit-second?
    w.show();
    // instance.setActivationWindow(&w);

    // set up a signal for SIGUSR1
    // signal(SIGUSR1, rebuildSystemMenuSignalHandler);
    QTimer delayedSearchFocus;
    delayedSearchFocus.setSingleShot(true);
    delayedSearchFocus.setInterval(200);
    QObject::connect(&delayedSearchFocus, &QTimer::timeout, &w,
                     [&w]() { w.m_MainWidget->triggerFocusMenu(); });
    QObject::connect(&instance, &QtSingleApplication::messageReceived,
                     [&delayedSearchFocus]() { delayedSearchFocus.start(); });

    if (!QDBusConnection::sessionBus().isConnected()) {
        fprintf(stderr,
                "Cannot connect to the D-Bus session bus.\n"
                "To start it, run:\n"
                "\teval `dbus-launch --auto-syntax`\n");
        return 1;
    }

    if (!QDBusConnection::sessionBus().registerService("local.Menu")) {
        fprintf(stderr, "%s\n", qPrintable(QDBusConnection::sessionBus().lastError().message()));
        exit(1);
    }

    QDBusConnection::sessionBus().registerObject("/", &w, QDBusConnection::ExportAllSlots);

    // probono: QUESTION: Why do we have to call this with
    // gdbus call --session --dest local.Menu --object-path / --method
    // local.Menu.MainWindow.showApplicationName "AppName" when the binary is named "Menu" but gdbus
    // call --session --dest local.Menu --object-path / --method
    // local.AppRun.MainWindow.showApplicationName "AppName" when the binary was launched through a
    // symlink called "AppRun"?
    //
    // probono: QUESTION: How can we avoid the string "MainWindow" from being part of the '--method'
    // argument? The name 'MainWindow' is a mere implementation detail and should not leak to the
    // outside world

    return instance.exec();
}
