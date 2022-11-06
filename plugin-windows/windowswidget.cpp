#include "windowswidget.h"
#include <QHBoxLayout>
#include <QLocale>
#include <QProcess>
#include <QDebug>
#include <QApplication>
#include <QPixmap>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <KF5/KWindowSystem/KWindowSystem>
#include <QtDBus/QDBusConnection>

#include "../src/applicationinfo.h"

WindowsWidget::WindowsWidget(QWidget *parent)
    : QWidget(parent),
      m_menubar(new QMenuBar),
      m_menu(new QMenu),
      m_refreshTimer(new QTimer(this))
{

    m_menu->setTitle(tr("Windows"));
    m_menu->menuAction()->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus

    QFont f = m_menu->menuAction()->font();
    f.setBold(true);
    m_menu->menuAction()->setFont(f);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setAlignment(Qt::AlignCenter); // Center QHBoxLayout vertically
    layout->setMargin(0);
    layout->setSpacing(0);
    layout->addWidget(m_menubar);
    m_menubar->addMenu(m_menu);
    setLayout(layout);

    updateWindows();

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &WindowsWidget::updateWindows);
    connect(KWindowSystem::self(), &KWindowSystem::windowRemoved, this, &WindowsWidget::updateWindows);

    connect(KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged)
            , this, &WindowsWidget::onWindowChanged);
}

void WindowsWidget::onWindowChanged(WId window, NET::Properties prop, NET::Properties2 prop2)
{
    if (prop & NET::WMState) {
        // Check whether a window is demanding attention
        KWindowInfo info(window, NET::WMState);
        if(info.hasState(NET::DemandsAttention)) {
            qDebug() << "probono: Window demands attention" << window;
            m_menu->menuAction()->setIcon(QIcon::fromTheme("dialog-warning")); // TODO: Blinking application icon
        }
    }
}

void WindowsWidget::updateWindows()

// FIXME: Instead of doing this whole thing each time the frontmost window changes
// we could keep some internal state and change just what needs to be changed;
// but writing the code for this would be significantly more complex
// and error-prone; so let's see whether we can get away with this.
// Looks like preformance is good since we use X11 atoms to store the kind of
// information we need directly on the windows themselves, so that we don't have
// to re-compute it each time windows are switched.

{
    ApplicationInfo *ai = new ApplicationInfo();
    m_menu->setTitle(ai->applicationNiceNameForWId(KWindowSystem::activeWindow()));

    m_menu->clear();

    m_menu->setToolTipsVisible(true);

    QList<unsigned int> distinctPids = {};
    QList<WId> distinctApps = {};

    // Find out one process ID per application that has at least one window
    // TODO: No Dock windows
    const QList<WId> winIds = KWindowSystem::windows();
    for (WId id : winIds){
        KWindowInfo info(id, NET::WMPid | NET::WMWindowType | NET::WMState, NET::WM2TransientFor | NET::WM2WindowClass | NET::WM2WindowRole);
        // Don't add the Dock and the Menu to this menu
        // but do add Desktop (even though it could be filtered away using NET::Desktop)
        NET::WindowTypes mask = NET::AllTypesMask;
        if (info.windowType(mask) & (NET::Dock))
            continue;
        if (info.windowType(mask) & (NET::Menu))
            continue;
        if (info.state() & NET::SkipTaskbar)
            continue;

        if (! distinctPids.contains(info.pid())) {
            distinctPids.append(info.pid());
            distinctApps.append(id);
        }
    }

    // Hide all windows of the process of the frontmost app
    // NOTE: We need to ensure that the keyboard shortcut is not already taken by .config/lxqt/globalkeyshortcuts.conf
    // if we want to use this here. But it seems like the shortcut set on this action never gets used?
    // So we may need to make a tiny "hide_application" command line tool?
    // Or register a GLOBAL shortcut in Qt?
    WId id = KWindowSystem::activeWindow();
    QAction *hideAction = m_menu->addAction(tr("Hide %1").arg(ai->applicationNiceNameForWId(id)));
    hideAction->setShortcut(QKeySequence("Ctrl+H"));

    connect(hideAction, &QAction::triggered, this, [hideAction, id, this]() {
        KWindowSystem::minimizeWindow(id);
        KWindowInfo info(id, NET::WMPid);
        const QList<WId> winIds = KWindowSystem::windows();
        for (WId cand_id : winIds){
            KWindowInfo cand_info(cand_id, NET::WMPid);
            if(cand_info.pid() == info.pid())
                KWindowSystem::minimizeWindow(cand_id);
        }
    });

    // Hide others
    QAction *hideOthersAction = m_menu->addAction(tr("Hide Others"));
    // hideOthersAction->setShortcut(QKeySequence("Shift+Alt+H"));
    connect(hideOthersAction, &QAction::triggered, this, [hideOthersAction, id, this]() {
        hideOthers(id);
    });

    // Show all
    QAction *showAllAction = m_menu->addAction(tr("Show All"));
    // showAllAction->setShortcut(QKeySequence("Shift+Ctrl+H"));
    connect(showAllAction, &QAction::triggered, this, [showAllAction, id, this]() {
        const QList<WId> winIds = KWindowSystem::windows();
        for (WId cand_id : winIds){
            KWindowSystem::unminimizeWindow(cand_id);
        }
    });

    m_menu->addSeparator();

    // Add one menu item for each appliction

    for (WId id : distinctApps){

        QString niceName = ai->applicationNiceNameForWId(id);

        // Do not show this Menu application itself in the list of windows
        KWindowInfo info(id, NET::WMPid);
        if (qApp->applicationPid() == info.pid())
            continue;

        // Find out how many menus there are for this PID
        int windowsForThisPID = 0;
        const QList<WId> winIds = KWindowSystem::windows();
        for (WId cand_id : winIds){
            KWindowInfo cand_info(cand_id, NET::WMPid);
            if(cand_info.pid() == info.pid())
                windowsForThisPID++;
        }

        // If there is only one window for this PID, then add a menu item (a QAction)
        if(windowsForThisPID <2) {
            QAction *appAction = m_menu->addAction(niceName);

            QPixmap pixmap;
            pixmap = KWindowSystem::icon(id, 24, 24, false);
            // Some applications don't set icons on their windows.
            // Yes, I am looking at you, Qt Creator, of all applications.
            // In this case, try to get one from the bundle
            if(pixmap.isNull()) {
                QString bPath = ai->bundlePathForWId(id);
                // TODO: To be moved to a function that gets the icon given a bundle path
                // .app
                const QString iconCand1 = QDir(bPath).canonicalPath() +
                        "/Resources/" + QFileInfo(bPath).completeBaseName() + ".png"; // TODO: Also check for other supported formats
                // .AppDir
                const QString iconCand2 = QDir(bPath).canonicalPath() +
                        "/.DirIcon";
                for(const QString iconCand : QStringList({iconCand1, iconCand2})) {
                    if(QFileInfo::exists(iconCand)) {
                        QImageReader r(iconCand);
                        r.setDecideFormatFromContent(true);
                        QImage i = r.read();
                        if (!i.isNull())
                            pixmap = QPixmap::fromImage(i);
                    }
                }
            }

            appAction->setIcon(QIcon(pixmap));

            // Call the Desktop the "Desktop"
            KWindowInfo info(id, NET::WMPid | NET::WMWindowType);
            if (info.windowType(NET::AllTypesMask) & (NET::Desktop)) {
                appAction->setText(tr("Desktop"));
                appAction->setIcon(QIcon::fromTheme("desktop"));
            }
#ifdef QT_DEBUG
            // For development and debugging, show information about the windows
            appAction->setToolTip(QString("Window ID: %1\n"
                                          "Bundle: %2\n"
                                          "Executable: %3").arg(id).arg(ai->bundlePathForWId(id)).arg(ai->pathForWId(id)));
#endif
            appAction->setCheckable(true);

            appAction->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus

            if(id == KWindowSystem::activeWindow()) {
                appAction->setChecked(true);
                appAction->setEnabled(false);
                m_menu->menuAction()->setIcon(QIcon(pixmap)); // Also set the icon for the overall Windows menu
            } else {
                connect(appAction, &QAction::triggered, this, [appAction, id, this]() {
                    WindowsWidget::activateWindow(id);
                });
            }
        } else {
            // If there are multiple windows for the same PID, then add a submenu (a QMenu with QActions)
            // So don't add an action here, but a submenu which contains all windows that beloong to that PID
            QMenu *subMenu = m_menu->addMenu(niceName);
            subMenu->setToolTipsVisible(true);

            QPixmap pixmap;
            pixmap = KWindowSystem::icon(id, 24, 24, false);
            subMenu->menuAction()->setIcon(QIcon(pixmap));


            subMenu->menuAction()->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus

            const QList<WId> winIds = KWindowSystem::windows();
            for (WId cand_id : winIds){
                KWindowInfo cand_info(cand_id, NET::WMPid | NET::WMName | NET::WMWindowType);
                if(cand_info.pid() == info.pid()) {
                    QAction *appAction = subMenu->addAction(cand_info.name());
                    // If more than one Filer window is open and one is the Desktop, call the Desktop the "Desktop"
                    if (cand_info.windowType(NET::AllTypesMask) & (NET::Desktop)) {
                        appAction->setText(tr("Desktop"));
                        subMenu->menuAction()->setIcon(QIcon::fromTheme("folder")); // TODO: Remove this once Filer sets its icon on the desktop window
                    }
#ifdef QT_DEBUG
                    // For development and debugging, show information about the windows
                    appAction->setToolTip(QString("Window ID: %1\n"
                                                  "Bundle: %2\n"
                                                  "Executable: %3").arg(cand_id).arg(ai->bundlePathForWId(cand_id)).arg(ai->pathForWId(cand_id)));
                    }
#endif
                    // appAction->setIcon(QIcon(KWindowSystem::icon(id))); // Why does this not work? TODO: Get icon from bundle?
                    if(cand_id == KWindowSystem::activeWindow()) {
                            appAction->setChecked(true);
                            appAction->setEnabled(false);
                            m_menu->menuAction()->setIcon(QIcon(pixmap)); // Also set the icon for the overall Windows menu
                    } else {
                        connect(appAction, &QAction::triggered, this, [appAction, cand_id, this]() {
                            WindowsWidget::activateWindow(cand_id);
                        });
                    }
                }
            }
        }
    }

    m_menu->addSeparator();

    // Show all
    QAction *fullscreenAction = m_menu->addAction(tr("Full Screen"));
    // TODO: Need a way to undo this...
    connect(fullscreenAction, &QAction::triggered, this, [fullscreenAction, id, this]() {
        KWindowSystem::setState(id, KWindowSystem::FullScreen);
    });
    ai->~ApplicationInfo();
}

void WindowsWidget::hideOthers(WId id) {
    // TODO: Should we be hiding not all other windows, but only windows belonging to other applications (PIDs)?
    const QList<WId> winIds = KWindowSystem::windows();
    for (WId cand_id : winIds){
        if(cand_id != id)
            KWindowSystem::minimizeWindow(cand_id);
    }
}

void WindowsWidget::activateWindow(WId id) {

    KWindowSystem::activateWindow(id);

    // If Filer has no windows open but is selected, show the desktop = hide all windows
    // _NET_WM_WINDOW_TYPE(ATOM) = _NET_WM_WINDOW_TYPE_DESKTOP
    KWindowInfo info(id, NET::WMPid | NET::WMWindowType);
    if (info.windowType(NET::AllTypesMask) & (NET::Desktop)) {
        qDebug() << "probono: Desktop selected, hence hiding all";
        const QList<WId> winIds = KWindowSystem::windows();
        for (WId cand_id : winIds) {
            KWindowSystem::minimizeWindow(cand_id);
        }
    }

    // If a modifier key is pressed, close all other open windows
    if (QApplication::keyboardModifiers()){
        qDebug() << "probono: Modifier key pressed, hence hiding all others";
        const QList<WId> winIds = KWindowSystem::windows();
        for (WId cand_id : winIds) {
            if(id != cand_id)
            KWindowSystem::minimizeWindow(cand_id);
        }
    }
}



