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
#include <csignal>

#include <qtsingleapplication/qtsingleapplication.h>

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
    if (instance.sendMessage("Wake up!"))
    return 0;

    // Application a(argc, argv); // probono: Use this instead of the next line for debugging
    QApplication a(argc, argv); // probono: Use this instead of the line above for production
    MainWindow w;
    w.show();
    window = &w;

    instance.setActivationWindow(&w);

    //set up a signal for SIGUSR1
    signal(SIGUSR1, rebuildSystemMenuSignalHandler);

    QObject::connect(&instance, SIGNAL(messageReceived(const QString&)),
             &w, SLOT(append(const QString&)));

    return instance.exec();
}
