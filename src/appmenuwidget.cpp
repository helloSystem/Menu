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

#include "appmenuwidget.h"
#include "appmenu/menuimporteradaptor.h"
#include <QProcess>
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QX11Info>
#include <QApplication>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QMessageBox>
#include <QStyle>

#include <KF5/KWindowSystem/KWindowSystem>
#include <KF5/KWindowSystem/KWindowInfo>
#include <KF5/KWindowSystem/NETWM>

#include "actionsearch/actionsearch.h"
#include "actionsearch/ui/dialog.h"

AppMenuWidget::AppMenuWidget(QWidget *parent)
    : QWidget(parent)
      // m_minButton(new QToolButton),
      // m_restoreButton(new QToolButton),
      // m_closeButton(new QToolButton)
      //m_buttonsAnimation(new QPropertyAnimation)
{
    // QProcess *process = new QProcess(this);
    // process->start("/usr/bin/gmenudbusmenuproxy", QStringList());

    QHBoxLayout *layout = new QHBoxLayout;
    setLayout(layout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // m_buttonsWidget = new QWidget(this);
    // QHBoxLayout *buttonsLayout = new QHBoxLayout(m_buttonsWidget);
    // buttonsLayout->setContentsMargins(0, 0, 0, 0);
    // buttonsLayout->setSpacing(3);
    // buttonsLayout->addWidget(m_closeButton);
    // buttonsLayout->addWidget(m_restoreButton);
    // buttonsLayout->addWidget(m_minButton);

    // QSize iconSize(32, 32);
    // m_minButton->setIcon(QIcon(":/resources/min.svg"));
    // m_restoreButton->setIcon(QIcon(":/resources/restore.svg"));
    // m_closeButton->setIcon(QIcon(":/resources/close.svg"));
    // m_minButton->setIconSize(iconSize);
    // m_restoreButton->setIconSize(iconSize);
    // m_closeButton->setIconSize(iconSize);

    // probono: Secondary QMenuBar for the leftmost menu
    // so that it does not interfere with the main menu.
    // TODO: Find a way to put this menu into the main menu bar rather than a secondary one.
    QMenuBar *leftmostMenuBar = new QMenuBar(this);
    leftmostMenuBar->setMaximumWidth(70);
    QMenu *leftmostMenu = leftmostMenuBar->addMenu("System");
    // leftmostMenu->setIcon(this->style()->standardIcon(QStyle::SP_ComputerIcon));
    QAction *aboutAction = leftmostMenu->addAction("About This Computer");
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(actionAbout()));

    QMenu *submenuPrefs = leftmostMenu->addMenu("Preferences");

    QAction *displaysAction = submenuPrefs->addAction("Displays");
    connect(displaysAction, SIGNAL(triggered()), this, SLOT(actionDisplays()));

    QAction *shortcutsAction = submenuPrefs->addAction("Shortcuts");
    connect(shortcutsAction, SIGNAL(triggered()), this, SLOT(actionShortcuts()));

    QAction *soundAction = submenuPrefs->addAction("Sound");
    connect(soundAction, SIGNAL(triggered()), this, SLOT(actionSound()));

    QAction *logoutAction = leftmostMenu->addAction("Log Out");
    connect(logoutAction, SIGNAL(triggered()), this, SLOT(actionLogout()));

    leftmostMenuBar->setAttribute(Qt::WA_TranslucentBackground);
    leftmostMenuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    leftmostMenuBar->setStyleSheet("background: transparent");
    layout->addWidget(leftmostMenuBar, 0, Qt::AlignVCenter);

    m_menuBar = new QMenuBar(this);
    m_menuBar->setAttribute(Qt::WA_TranslucentBackground);
    m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_menuBar->setStyleSheet("background: transparent");
    m_menuBar->setFont(qApp->font());
    // layout->addWidget(m_buttonsWidget, 0, Qt::AlignVCenter);

    // Add search box to menu
    QLineEdit *searchLineEdit = new QLineEdit;
    searchLineEdit->setEnabled(true);
    searchLineEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    searchLineEdit->setText("TODO: Action Search");
    searchLineEdit->setStyleSheet("color: red; border-radius: 5px");

    layout->addWidget(m_menuBar, 0, Qt::AlignLeft);

    layout->addSpacing(10);
    layout->addWidget(searchLineEdit);

    layout->setContentsMargins(0, 0, 0, 0);

//    m_buttonsAnimation->setTargetObject(m_buttonsWidget);
//    m_buttonsAnimation->setPropertyName("maximumWidth");
//    m_buttonsAnimation->setDuration(300);
//    m_buttonsWidth = m_buttonsWidget->width();

    MenuImporter *menuImporter = new MenuImporter(this);
    menuImporter->connectToBus();

    m_appMenuModel = new AppMenuModel(this);
    connect(m_appMenuModel, &AppMenuModel::modelNeedsUpdate, this, &AppMenuWidget::updateMenu);

//    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppMenuWidget::delayUpdateActiveWindow);
//    connect(KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
//            this, &AppMenuWidget::onWindowChanged);

    // connect(m_minButton, &QToolButton::clicked, this, &AppMenuWidget::minimizeWindow);
    // connect(m_closeButton, &QToolButton::clicked, this, &AppMenuWidget::clsoeWindow);
    // connect(m_restoreButton, &QToolButton::clicked, this, &AppMenuWidget::restoreWindow);

    delayUpdateActiveWindow();

    // Load action search
    // FIXME: This needs to somehow be integrated with the AppMenuModel. How to do this?
    actionSearch = new ActionSearch(leftmostMenuBar); // probono: FIXME: Kinda works with leftmostMenuBar but not yet with m_menuBar
    actionSearch->update();
    actionDialog = new Dialog{this, actionSearch->getActionNames()};
    
    connect(actionDialog, &QDialog::accepted, this, &AppMenuWidget::acceptActionDialog);

    actionDialog->setModal(false);
    actionDialog->setParent(this, Qt::Dialog); // setParent to this results in the menu not going away when the dialog is shown
    actionDialog->show();
}

void AppMenuWidget::acceptActionDialog() {
    actionSearch->execute(actionDialog->getActionName());
}

void AppMenuWidget::updateMenu()
{
    m_menuBar->clear();

    if (!m_appMenuModel->menuAvailable()) {
        return;
    }

    QMenu *menu = m_appMenuModel->menu();
    if (menu) {
        for (QAction *a : menu->actions()) {
            if (!a->isEnabled())
                continue;

            m_menuBar->addAction(a);
        }
    }
}

void AppMenuWidget::toggleMaximizeWindow()
{
    KWindowInfo info(KWindowSystem::activeWindow(), NET::WMState);
    bool isMax = info.hasState(NET::Max);
    bool isWindow = !info.hasState(NET::SkipTaskbar) ||
            info.windowType(NET::UtilityMask) != NET::Utility ||
            info.windowType(NET::DesktopMask) != NET::Desktop;

    if (!isWindow)
        return;

    if (isMax) {
        restoreWindow();
    } else {
        maxmizeWindow();
    }
}

bool AppMenuWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ApplicationFontChange) {
        QMenu *menu = m_appMenuModel->menu();
        if (menu) {
            for (QAction *a : menu->actions()) {
                a->setFont(qApp->font());
            }
        }
        qDebug() << "gengxinle  !!!" << qApp->font().toString();
        m_menuBar->setFont(qApp->font());
        m_menuBar->update();
        // actionSearch->update();
    }

    return QWidget::event(e);
}

bool AppMenuWidget::isAcceptWindow(WId id)
{
    QFlags<NET::WindowTypeMask> ignoreList;
    ignoreList |= NET::DesktopMask;
    ignoreList |= NET::DockMask;
    ignoreList |= NET::SplashMask;
    ignoreList |= NET::ToolbarMask;
    ignoreList |= NET::MenuMask;
    ignoreList |= NET::PopupMenuMask;
    ignoreList |= NET::NotificationMask;

    KWindowInfo info(id, NET::WMWindowType | NET::WMState, NET::WM2TransientFor | NET::WM2WindowClass);

    if (!info.valid())
           return false;

   if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), ignoreList))
       return false;

   if (info.state() & NET::SkipTaskbar)
       return false;

   // WM_TRANSIENT_FOR hint not set - normal window
   WId transFor = info.transientFor();
   if (transFor == 0 || transFor == id || transFor == (WId) QX11Info::appRootWindow())
       return true;

   info = KWindowInfo(transFor, NET::WMWindowType);

   QFlags<NET::WindowTypeMask> normalFlag;
   normalFlag |= NET::NormalMask;
   normalFlag |= NET::DialogMask;
   normalFlag |= NET::UtilityMask;

   return !NET::typeMatchesMask(info.windowType(NET::AllTypesMask), normalFlag);
}

void AppMenuWidget::delayUpdateActiveWindow()
{
    // if (m_windowID == KWindowSystem::activeWindow())
    //     return;

    // m_windowID = KWindowSystem::activeWindow();

    // onActiveWindowChanged();
}

void AppMenuWidget::onActiveWindowChanged()
{
    KWindowInfo info(m_windowID, NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);
    bool isMax = info.hasState(NET::Max);

//    m_buttonsAnimation->stop();

    if (isAcceptWindow(m_windowID) && isMax) {
//        m_buttonsAnimation->setStartValue(m_buttonsWidget->width());
//        m_buttonsAnimation->setEndValue(m_buttonsWidth);
//        m_buttonsAnimation->setEasingCurve(QEasingCurve::InOutCubic);
//        m_buttonsAnimation->start();
        m_buttonsWidget->setVisible(true);
    } else {
//        m_buttonsAnimation->setStartValue(m_buttonsWidget->width());
//        m_buttonsAnimation->setEndValue(0);
//        m_buttonsAnimation->setEasingCurve(QEasingCurve::InOutCubic);
//        m_buttonsAnimation->start();
        m_buttonsWidget->setVisible(false);
    }
}

void AppMenuWidget::onWindowChanged(WId /*id*/, NET::Properties /*properties*/, NET::Properties2 /*properties2*/)
{
    if (m_windowID == KWindowSystem::activeWindow())
        onActiveWindowChanged();
}

void AppMenuWidget::minimizeWindow()
{
    KWindowSystem::minimizeWindow(KWindowSystem::activeWindow());
}

void AppMenuWidget::clsoeWindow()
{
    NETRootInfo(QX11Info::connection(), NET::CloseWindow).closeWindowRequest(KWindowSystem::activeWindow());
}

void AppMenuWidget::maxmizeWindow()
{
    KWindowSystem::setState(KWindowSystem::activeWindow(), NET::Max);
}

void AppMenuWidget::restoreWindow()
{
    KWindowSystem::clearState(KWindowSystem::activeWindow(), NET::Max);
}

void AppMenuWidget::actionAbout()
{
    qDebug() << "actionAbout() called";

    QString translatedTextAboutQtCaption;
    translatedTextAboutQtCaption = "<h3>About This Computer</h3>";
    QMessageBox *msgBox = new QMessageBox(QMessageBox::NoIcon, "Title", "Text");
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle("About This Computer");
    msgBox->setText("Kernel information goes here. To be implemented.");
    msgBox->setInformativeText("SMBIOS information goes here. To be implemented.");
    msgBox->setParent(this, Qt::Dialog); // setParent to this results in the menu not going away when the dialog is shown

    // On FreeBSD, get information about the machine
    if(which("kenv")){
        QProcess p;
        QString program = "kenv";
        QStringList arguments;
        arguments << "-q" << "smbios.system.maker";
        p.start(program, arguments);
        p.waitForFinished();
        QString vendorname(p.readAllStandardOutput());
        vendorname.replace("\n", "");
        vendorname = vendorname.trimmed();
        qDebug() << "vendorname:" << vendorname;
        QStringList arguments2;
        arguments2 << "-q" << "smbios.system.product";
        p.start(program, arguments2);
        p.waitForFinished();
        QString productname(p.readAllStandardOutput());
        productname.replace("\n", "");
        productname = productname.trimmed();
        qDebug() << "systemname:" << productname;
        msgBox->setInformativeText(vendorname + " " + productname);
        QString program2 = "uname";
        QStringList arguments3;
        arguments3 << "-v";
        p.start(program2, arguments3);
        p.waitForFinished();
        QString operatingsystem(p.readAllStandardOutput());
        operatingsystem.replace("\n", "");
        operatingsystem = operatingsystem.trimmed();
        qDebug() << "systemname:" << operatingsystem;
        msgBox->setText(operatingsystem);
    }

    QSize sz(48, 48);
    msgBox->setIconPixmap(style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(sz));

    msgBox->exec();
}

void AppMenuWidget::actionDisplays()
{
    qDebug() << "actionDisplays() called";
    // TODO: Find working Qt based tool
    if(which("arandr")) {
        QProcess::startDetached("arandr"); // sudo pkg install arandr // Gtk
    } else if (which("lxrandr")) {
        QProcess::startDetached("lxrandr"); // sudo pkg install lxrandr // Gtk
    } else {
        qDebug() << "arandr, lxrandr not found";
    }
}

void AppMenuWidget::actionShortcuts()
{
    qDebug() << "actionShortcuts() called";
    QProcess::startDetached("lxqt-config-globalkeyshortcuts");
}

void AppMenuWidget::actionSound()
{
    qDebug() << "actionSound() called";
    QProcess::startDetached("dsbmixer");
}

void AppMenuWidget::actionLogout()
{
    qDebug() << "actionLogout() called";
    // Check if we have the Shutdown binary at hand
    if(QFileInfo(QCoreApplication::applicationDirPath() + QString("/Shutdown")).isExecutable()) {
    QProcess::execute(QCoreApplication::applicationDirPath() + QString("/Shutdown"));
    } else {
        qDebug() << "Shutdown executable not available next to Menubar executable, exiting";
        QApplication::exit(); // In case we are lacking the Shutdown executable
    }
}

bool AppMenuWidget::which(QString command)
{
    QProcess findProcess;
    QStringList arguments;
    arguments << command;
    findProcess.start("which", arguments);
    findProcess.setReadChannel(QProcess::ProcessChannel::StandardOutput);

    if(!findProcess.waitForFinished())
        return false; // Not found or which does not work

    QString retStr(findProcess.readAll());

    retStr = retStr.trimmed();

    QFile file(retStr);
    QFileInfo check_file(file);
    if (check_file.exists() && check_file.isFile())
        return true; // Found!
    else
        return false; // Not found!
}
