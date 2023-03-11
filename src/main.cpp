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
#include <csignal>

#include <qtsingleapplication/qtsingleapplication.h>

#include <QLibraryInfo>

//our main window
MainWindow* window;

// probono: Subclassing QApplication so that we can see what events are going on
// https://stackoverflow.com/a/27607947
class Application final : public QApplication {
public:
    Application(int& argc, char** argv) : QApplication(argc, argv) {}
    virtual bool notify(QObject *receiver, QEvent *e) override {
         // your code here
        qDebug() << "probono: e.type():" << e->type();
        // qDebug() << "probono: QApplication::focusWidget():" << QApplication::focusWidget();
        if (QApplication::focusWidget() != nullptr) {
            qDebug() << "probono: QApplication::focusWidget()->objectName():" << QApplication::focusWidget()->objectName();
            qDebug() << "probono: QApplication::focusWidget()->metaObject()->className():" << QApplication::focusWidget()->metaObject()->className();
        }
        return QApplication::notify(receiver, e);
    }
};

//Rebuild the system menu on SIGUSR1 
//https://github.com/helloSystem/Menu/issues/16
void rebuildSystemMenuSignalHandler(int sig){
    window->m_mainPanel->rebuildSystemMenu();
}


// probono: Using QtSingleApplication so that only one instance can run at any time,
// launching it again just brings the running instance into focus
// https://github.com/qtproject/qt-solutions/blob/master/qtsingleapplication/examples/trivial/main.cpp

int main(int argc, char **argv)
{
    QtSingleApplication instance(argc, argv);
    if (instance.sendMessage("Wake up!")) {
        return 0;
    }

    QApplication *a = new QApplication(argc, argv); // probono: Use this instead of the next line for debugging
    // Application *a = new Application(argc, argv); // probono: Use this instead of the line above for production

    QTranslator *qtTranslator = new QTranslator(a);
    QTranslator *translator = new QTranslator(a);

    // Install the translations built-into Qt itself
    qDebug() << "probono: QLocale::system().name()" << QLocale::system().name();
    if (! qtTranslator->load("qt_" + QLocale::system().name(), QLibraryInfo::location(QLibraryInfo::TranslationsPath))){
        // Other than qDebug, qCritical also works in Release builds
        qCritical() << "Failed qtTranslator->load";
    } else {
        if (! qApp->installTranslator(qtTranslator)){
            qCritical() << "Failed qApp->installTranslator(qtTranslator)";
        }
    }

    // Install our own translations
    if (! translator->load("menubar_" + QLocale::system().name(), QCoreApplication::applicationDirPath() + QString("/../share/menubar/translations/"))) { // probono: FHS-like path relative to main binary
        qDebug() << "probono: loading translations from FHS tree not successful";
        if (! translator->load("menubar_" + QLocale::system().name(), QCoreApplication::applicationDirPath())) { // probono: When qm files are next to the executable ("uninstalled"), useful during development
            qCritical() << "Failed translator->load";
        }
    }

    if (! translator->isEmpty()) {
        if (! qApp->installTranslator(translator)){
            qCritical() << "Failed qApp->installTranslator(translator)";
        }
    }

    MainWindow w;
    window = &w;
    QTimer::singleShot(500, window, &MainWindow::show); // probono: Will this prevent the menu from showing up in random places for a slit-second?

    instance.setActivationWindow(&w);

    //set up a signal for SIGUSR1
    signal(SIGUSR1, rebuildSystemMenuSignalHandler);

    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &w, SLOT(append(const QString&)));

    return instance.exec();
}
