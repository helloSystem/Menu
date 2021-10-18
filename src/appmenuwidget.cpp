/*
 * Copyright (C) 2020 PandaOS Team.
 * Author:     rekols <revenmartin@gmail.com>
 * Portions Copyright (C) 2020 Simon Peter.
 * Author:     Simon Peter <probono@puredarwin.org>
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
#include "mainpanel.h"
#include <QProcess>
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QX11Info>
#include <QApplication>

#include <QAbstractItemView>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPushButton>
#include <QStyle>
#include <QDesktopWidget>
#include <QScreen>
#include <QObject>
#include <QStandardPaths>
#include <QMouseEvent>
#include <QTimer>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QListView>

#include <KF5/KWindowSystem/KWindowSystem>
#include <KF5/KWindowSystem/KWindowInfo>
#include <KF5/KWindowSystem/NETWM>

#include "actionsearch/actionsearch.h"

#if defined(Q_OS_FREEBSD)
#include <sys/types.h>
#include <sys/extattr.h>
#endif

class MyLineEditEventFilter : public QObject
{
public:
    explicit MyLineEditEventFilter(QLineEdit *parent) : QObject(parent)
    {}

    bool eventFilter(QObject *obj, QEvent *e)
    {
        // qDebug() << "probono: e->type()" << e->type();
        switch (e->type())
        {
        case QEvent::WindowActivate:
        {
            // Whenever this window becomes active, then set the focus on the search box
            if(reinterpret_cast<QLineEdit *>(parent())->hasFocus() == false){
                reinterpret_cast<QLineEdit *>(parent())->setFocus();
            }
            break;
        }
        case QEvent::KeyPress:
        {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
            // qDebug() << "probono: keyEvent->key()" << keyEvent->key();
            if (keyEvent->key() == Qt::Key_Escape)
            {
                // When esc key is pressed while cursor is in QLineEdit, empty the QLineEdit
                // https://stackoverflow.com/a/38066410
                reinterpret_cast<QLineEdit *>(parent())->clear();
                reinterpret_cast<QLineEdit *>(parent())->setText("");
            }
            if (keyEvent->key() == Qt::Key_Tab | Qt::Key_Alt)
            {
                // When esc Tab is pressed while cursor is in QLineEdit, also empty the QLineEdit
                // and prevent the focus from going elsewhere in the menu. This effectively prevents the menu
                // from being operated by cursor keys. If we want that functionality back, we might remove
                // the handling of Qt::Key_Tab but instead we would have to ensure that we put the focus back
                // on the search box whenever this application is launched (again) and re-invoked by QSingleApplication
                reinterpret_cast<QLineEdit *>(parent())->clear();
                reinterpret_cast<QLineEdit *>(parent())->setText("");
            }
            break;
        }
        case QEvent::FocusOut: // QEvent::FocusOut:
        {
            // When the focus goes not of the QLineEdit, empty the QLineEdit and restore the placeholder text
            // reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText("Alt+Space");
            reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText("Search");
            // Note that we write Alt-Space here but in fact this is not a feature of this application
            // but is a feature of lxqt-config-globalkeyshortcuts in our case, where we set up a shortcut
            // that simply launches this application (again). Since we are using
            // searchLineEdit->setStyleSheet("background: white"); // Do this in stylesheet.qss instead
            // reinterpret_cast<QLineEdit *>(parent())->setAlignment(Qt::AlignmentFlag::AlignRight);
            reinterpret_cast<QLineEdit *>(parent())->clear();
            reinterpret_cast<QLineEdit *>(parent())->setText("");
            break;
        }
        case QEvent::FocusIn:
        {
            // When the focus goes into the QLineEdit, empty the QLineEdit
            reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText("");
            // reinterpret_cast<QLineEdit *>(parent())->setAlignment(Qt::AlignmentFlag::AlignLeft);
            break;
        }
        }
        return QObject::eventFilter(obj, e);
    }
};

// Select the first result of the completer if there is only one result left
// https://github.com/helloSystem/Menu/issues/14
class AutoSelectFirstFilter : public QObject
{
public:
    explicit AutoSelectFirstFilter(QLineEdit *parent) : QObject(parent)
    {}

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        QCompleter *completer = reinterpret_cast<QLineEdit *>(parent())->completer();
        // Automatically select the first match of the completer if there is only one result left

        if (e->type() == QEvent::KeyRelease)
        {
            if(completer->completionCount() == 1){
                // completer->setCurrentRow(0); // This is not changing the current row selection, but the following does
                QListView* l = static_cast<QListView*>(completer->popup());
                QModelIndex idx = completer->completionModel()->index(0, 0,QModelIndex());
                l->setCurrentIndex(idx);
            }
        }
        return QObject::eventFilter(obj, e);
    }
};

void AppMenuWidget::findAppsInside(QStringList locationsContainingApps, QMenu *m_systemMenu, QFileSystemWatcher *watcher)
// probono: Check locationsContainingApps for applications and add them to the m_systemMenu.
// TODO: Nested submenus rather than flat ones with '→'
// This code is similar to the code in the 'launch' command
{
    QStringList nameFilter({"*.app", "*.AppDir", "*.desktop"});
    foreach (QString directory, locationsContainingApps) {
        // Shall we process this directory? Only if it contains at least one application, to optimize for speed
        // by not descending into directory trees that do not contain any applications at all. Can make
        // a big difference.

        QDir dir(directory);
        int numberOfAppsInDirectory = dir.entryList(nameFilter).length();
        QMenu *submenu;

        if(directory.endsWith(".app") == false && directory.endsWith(".AppDir") == false && numberOfAppsInDirectory > 0) {
            qDebug() << "# Descending into" << directory;
            QStringList locationsToBeChecked = {directory};
            QFileInfo fi(directory);
            QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers
            // submenu = m_systemMenu->addMenu(base); // TODO: Use this once we have nested submenus rather than flat ones with '→'            
            submenu = m_systemMenu->addMenu(directory);
            submenu->setProperty("path", directory);

            // https://github.com/helloSystem/Menu/issues/15
            // probono: Watch this directory for changes and if we detect any, rebuild the menu
            watcher->addPath(directory);

            submenu->setToolTip(directory);
            submenu->setTitle(directory.remove(0, 1).replace("/", " → "));
            submenu->setToolTipsVisible(true); // Seems to be needed here, too, so that the submenu items show their correct tooltips?
            // Make it possible to open the directory that contains the app by clicking on the submenu itself
            qDebug() << "probono: Installing event filter";
            submenu->installEventFilter(this);
            connect(submenu, SIGNAL(triggered(QAction*)), SLOT(actionLaunch(QAction*)));
        } else {
            continue;
        }

        // Use QDir::entryList() insted of QDirIterator because it supports sorting
        QStringList candidates = dir.entryList();
        QString candidate;
        foreach(candidate, candidates ) {
            candidate = dir.path() + "/" + candidate;
            // Do not show Autostart directories (or should we?)
            if (candidate.endsWith("/Autostart") == true) {
                    continue;
            }
            // qDebug() << "probono: Processing" << candidate;
            QString nameWithoutSuffix = QFileInfo(QDir(candidate).canonicalPath()).completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers; dereference symlink to candidate
            QFileInfo file(candidate);
            if (file.fileName().endsWith(".app")){
                QString AppCand = QDir(candidate).canonicalPath() + "/" + nameWithoutSuffix; // Dereference symlink to candidate
                // qDebug() << "################### Checking" << AppCand;
                if(QFileInfo(AppCand).exists() == true) {
                    qDebug() << "# Found" << AppCand;
                    QFileInfo fi(file.fileName());
                    QString base = fi.completeBaseName();  // The name of the .app directory without suffix // baseName() gets it wrong e.g., when there are dots in version numbers
                    QAction *action = submenu->addAction(base);
                    action->setToolTip(file.absoluteFilePath());
                    action->setProperty("path", file.absoluteFilePath());
                    QString IconCand = QDir(candidate).canonicalPath() + "/Resources/" + nameWithoutSuffix + ".png";
                    if(QFileInfo(IconCand).exists() == true) {
                        // qDebug() << "#   Found icon" << IconCand;
                        action->setIcon(QIcon(IconCand));
                        action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus
                    }
                }
            }
            else if (file.fileName().endsWith(".AppDir")) {
                QString AppCand = QDir(candidate).canonicalPath() + "/" + "AppRun";
                // qDebug() << "################### Checking" << AppCand;
                if(QFileInfo(AppCand).exists() == true){
                    qDebug() << "# Found" << AppCand;
                    QFileInfo fi(file.fileName());
                    QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers
                    QStringList executableAndArgs = {AppCand};
                    QAction *action = submenu->addAction(base);
                    action->setToolTip(file.absoluteFilePath());
                    action->setProperty("path", file.absoluteFilePath());
                    QString IconCand = QDir(candidate).canonicalPath() + "/.DirIcon";
                    if(QFileInfo(IconCand).exists() == true) {
                        // qDebug() << "#   Found icon" << IconCand;
                        action->setIcon(QIcon(IconCand));
                        action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus
                    }
                }
            }
            else if (file.fileName().endsWith(".desktop")) {
                // .desktop file
                qDebug() << "# Found" << file.fileName() << "TODO: Parse it for Exec=";
                QFileInfo fi(file.fileName());
                QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers
                QStringList executableAndArgs = {fi.absoluteFilePath()};
                QAction *action = submenu->addAction(base);
                action->setToolTip("TODO: Convert " + file.absoluteFilePath() + " to an .app bundle");
                action->setProperty("path", file.absoluteFilePath());
                action->setDisabled(true); // As a reminder that we consider those legacy and encourage people to switch
                // Finding the icon file is much more involved with XDG than with our simplified .app bundles, so it is not implemented here
            }
            else if (locationsContainingApps.contains(candidate) == false && file.isDir() && candidate.endsWith("/..") == false && candidate.endsWith("/.") == false && candidate.endsWith(".app") == false && candidate.endsWith(".AppDir") == false) {
                qDebug() << "# Found" << file.fileName() << ", a directory that is not an .app bundle nor an .AppDir";
                QStringList locationsToBeChecked({candidate});
                findAppsInside(locationsToBeChecked, m_systemMenu, watcher);
            }
        }
    }
}

AppMenuWidget::AppMenuWidget(QWidget *parent)
    : QWidget(parent)
{
    // probono: Reload menu when something changed in a watched directory; FIXME: This is not functional yet
    // https://github.com/helloSystem/Menu/issues/15
    watcher = new QFileSystemWatcher(this);
    // watcher->connect(watcher, SIGNAL(directoryChanged(QString)), this, SLOT(updateMenu())); // We need a slot that rebuilds the menu
    connect(watcher, SIGNAL(directoryChanged(QString)), SLOT(rebuildMenu()));                // We need a slot that rebuilds the menu

    QHBoxLayout *layout = new QHBoxLayout;
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    // setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Add search box to menu
    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setObjectName("actionSearch"); // probono: This name can be used in qss to style it specifically
    searchLineEdit->setPlaceholderText("Search");
    auto* pLineEditEvtFilter = new MyLineEditEventFilter(searchLineEdit);
    searchLineEdit->installEventFilter(pLineEditEvtFilter);
    // searchLineEdit->setMinimumWidth(150);
    searchLineEdit->setFixedWidth(75);
    // searchLineEdit->setStyleSheet("border-radius: 9px"); // We do this in the stylesheet.qss instead
    searchLineEdit->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false);
    // searchLineEdit->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    // Try to get the focus so that one can start typing immediately whenever the Menu is invoked
    // https://stackoverflow.com/questions/526761/set-qlineedit-focus-in-qt
    searchLineEdit->setFocusPolicy(Qt::StrongFocus); // searchLineEdit->setFocus(); alone does not always succeed
    searchLineEdit->setFocus();
    searchLineEdit->setFocus(Qt::OtherFocusReason); // searchLineEdit->setFocus(); alone does not always succeed
    searchLineEdit->setToolTip("Alt+Space"); // This is actually a feature not of this application, but some other application that merely launches this application upon Alt+Space
    // layout->addSpacing(10); // Space to the left before the searchLineWidget
    searchLineWidget = new QWidget(this);
    // searchLineWidget->setWindowFlag(Qt::WindowDoesNotAcceptFocus, true); // Does not seem to do anything
    auto searchLineLayout = new QHBoxLayout(searchLineWidget);
    searchLineLayout->setContentsMargins(0, 0, 0, 0);
    // searchLineLayout->setSpacing(3);
    searchLineLayout->addWidget(searchLineEdit, 0, Qt::AlignLeft);
    // searchLineWidget->setLayout(searchLineLayout);
    searchLineWidget->setObjectName("SearchLineWidget");
    // layout->addWidget(searchLineWidget, 0, Qt::AlignRight);
    layout->addWidget(searchLineWidget, 0, Qt::AlignLeft);
    searchLineWidget->show();

    // Prepare System menu
    m_systemMenu = new QMenu("System");
    m_systemMenu->setToolTipsVisible(true); // Works; shows the full path
    QAction *aboutAction = m_systemMenu->addAction(tr("About This Computer"));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(actionAbout()));
    m_systemMenu->addSeparator();
    // TODO: Move to a separate "Windows" (sub-)menu?
    QAction *minimizeAllAction = m_systemMenu->addAction(tr("Hide all"));
    connect(minimizeAllAction, SIGNAL(triggered()), this, SLOT(actionMinimizeAll()));
    QAction *maximizeAllAction = m_systemMenu->addAction(tr("Unhide all"));
    connect(maximizeAllAction, SIGNAL(triggered()), this, SLOT(actionMaximizeAll()));
    m_systemMenu->addSeparator();
    // Add submenus with applications to the System menu
    QStringList locationsContainingApps = {};
    locationsContainingApps.append(QDir::homePath());
    locationsContainingApps.append(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    locationsContainingApps.append(QDir::homePath() + "/Applications");
    locationsContainingApps.append(QDir::homePath() + "/bin");
    locationsContainingApps.append(QDir::homePath() + "/.bin");
    locationsContainingApps.append("/Applications");
    locationsContainingApps.removeDuplicates(); // Make unique
    findAppsInside(locationsContainingApps, m_systemMenu, watcher);
    m_systemMenu->addSeparator();
    QAction *restartAction = m_systemMenu->addAction(tr("Restart"));
    connect(restartAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    QAction *logoutAction = m_systemMenu->addAction(tr("Log Out"));
    connect(logoutAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    QAction *shutdownAction = m_systemMenu->addAction(tr("Shut Down"));
    connect(shutdownAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    // Add main menu
    m_menuBar = new QMenuBar(this);

    // m_menuBar->setStyleSheet("padding: 0px; padding: 0px;");
    m_menuBar->setContentsMargins(0, 0, 0, 0);
    m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding); // Naming is counterintuitive. "Maximum" keeps its size to a minimum! Need "Expanding" in y direction so that font will be centered

    integrateSystemMenu(m_menuBar); // Add System menu to main menu
    layout->addWidget(m_menuBar, 0, Qt::AlignLeft);
    layout->insertStretch(2); // Stretch after the main menu, which is the 2nd item in the layout

    // Action Search
    MenuImporter *menuImporter = new MenuImporter(this);
    menuImporter->connectToBus();

    m_appMenuModel = new AppMenuModel(this);
    connect(m_appMenuModel, &AppMenuModel::modelNeedsUpdate, this, &AppMenuWidget::updateMenu);

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppMenuWidget::delayUpdateActiveWindow);
    connect(KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
            this, &AppMenuWidget::onWindowChanged);

    // Load action search
    actionSearch = nullptr;
    actionCompleter = nullptr;
    updateActionSearch(m_menuBar);
}

AppMenuWidget::~AppMenuWidget() {
    if(actionSearch) {
        delete actionSearch;
    }
}

void AppMenuWidget::integrateSystemMenu(QMenuBar *menuBar) {
    if(!menuBar || !m_systemMenu)
        return;

    menuBar->addMenu(m_systemMenu);
}

void AppMenuWidget::handleActivated(const QString &name) {
    actionSearch->execute(name);
}

void AppMenuWidget::updateActionSearch(QMenuBar *menuBar) {
    if(!menuBar){
        return;
    }

    if(!actionSearch) {
        actionSearch = new ActionSearch;
    }

    /// Update the action search.
    actionSearch->clear();
    actionSearch->update(menuBar);

    /// Update completer
    if(actionCompleter) {
        actionCompleter->disconnect();
        actionCompleter->deleteLater();
    }

    actionCompleter = new QCompleter(actionSearch->getActionNames(), this);
    // TODO: https://stackoverflow.com/a/33790639
    // We could customize more aspects of the list view of the completer by
    //setting the CompletionMode to InlineCompletion, so there will be no popup.
    // Then make your QListView independent of the QLineEdit;
    // just react to signals that indicate when a view types some text,...

    KWindowSystem::setType(actionCompleter->popup()->winId(), NET::DropdownMenu);

    actionCompleter->popup()->setObjectName("actionCompleterPopup");
    // static_cast<QListView *>(actionCompleter->popup())->setSpacing(10);
    // static_cast<QListView *>(actionCompleter->popup())->setUniformItemSizes(true);
    // static_cast<QListView *>(actionCompleter->popup())->setContentsMargins(10,10,0,10); // FIXME: Does not seem to work, why?

    // Empty search field on selection of an item, https://stackoverflow.com/a/11905995
    QObject::connect(actionCompleter, SIGNAL(activated(const QString&)),
                     searchLineEdit, SLOT(clear()),
                     Qt::QueuedConnection);

    // Make more than 7 items visible at once
    actionCompleter->setMaxVisibleItems(35);

    // compute needed width
    // const QAbstractItemView * popup = actionCompleter->popup();
    // actionCompleter->popup()->setMinimumWidth(350);
    actionCompleter->popup()->setMinimumWidth(600);

    actionCompleter->popup()->setContentsMargins(100,100,100,100);

    // Make the completer match search terms in the middle rather than just those at the beginning of the menu
    actionCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    actionCompleter->setFilterMode(Qt::MatchContains);

    // Set first result active; https://stackoverflow.com/q/17782277. FIXME: This does not work yet. Why?
    QItemSelectionModel* sm = new QItemSelectionModel(actionCompleter->completionModel());
    actionCompleter->popup()->setSelectionModel(sm);
    sm->select(actionCompleter->completionModel()->index(0,0), QItemSelectionModel::Select);

    auto* flt = new AutoSelectFirstFilter(searchLineEdit);
    actionCompleter->popup()->installEventFilter(flt);

    actionCompleter->popup()->setAlternatingRowColors(false);
    // actionCompleter->popup()->setStyleSheet("QListView::item { color: green; }"); // FIXME: Does not work. Why?
    searchLineEdit->setCompleter(actionCompleter);

    // Sort results of the Action Search
    actionCompleter->completionModel()->sort(0,Qt::SortOrder::AscendingOrder);

    connect(actionCompleter,
            QOverload<const QString &>::of(&QCompleter::activated),
            this,
            &AppMenuWidget::handleActivated);
}


void AppMenuWidget::rebuildMenu()
{
    qobject_cast<MainPanel*>(parent())->rebuildSystemMenu();
    qDebug() << "AppMenuWidget::rebuildMenu() called";
}

//doesn't work for https://github.com/helloSystem/Menu/issues/16
//what does this even do??
void AppMenuWidget::updateMenu()
{
    // qDebug() << "AppMenuWidget::updateMenu() called";
    m_menuBar->clear();
    integrateSystemMenu(m_menuBar); // Insert the 'System' menu first

    if (!m_appMenuModel->menuAvailable()) {
        updateActionSearch(m_menuBar);
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

    updateActionSearch(m_menuBar);
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
        maximizeWindow();
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
    if (m_windowID == KWindowSystem::activeWindow())
        return;

    m_windowID = KWindowSystem::activeWindow();

    onActiveWindowChanged();
}

void AppMenuWidget::onActiveWindowChanged()
{
    KWindowInfo info(m_windowID, NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);
    // bool isMax = info.hasState(NET::Max);
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

void AppMenuWidget::closeWindow()
{
    NETRootInfo(QX11Info::connection(), NET::CloseWindow).closeWindowRequest(KWindowSystem::activeWindow());
}

void AppMenuWidget::maximizeWindow()
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
    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setWindowTitle("About This Computer");

    QString url;
    QString sha;
    QString build;
    
#if defined(Q_OS_FREEBSD)
    // Try to get extended attributes on the /.url file
    if (QFile::exists("/.url")) {
        url = nullptr;
        char buf[256] = "";
        if (extattr_get_file("/.url", EXTATTR_NAMESPACE_USER, "url", buf, 256) > 0) {
            url = QString(buf);
        }
        char buf2[128] = "";
        qDebug() << "extattr 'url' from '/.url':" << url;
        sha = nullptr;
        if (extattr_get_file("/.url", EXTATTR_NAMESPACE_USER, "sha", buf2, 128) > 0) {
            sha = QString(buf2);
        }
        qDebug() << "extattr 'sha' from '/.url':" << sha;

        char buf3[128] = "";
        build = nullptr;
        if (extattr_get_file("/.url", EXTATTR_NAMESPACE_USER, "build", buf3, 128) > 0) {
            build = QString(buf3);
        }
        qDebug() << "extattr 'build' from '/.url':" << build;
    }
#endif

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
        msgBox->setText("<b>" + vendorname + " " + productname + "</b>");

        QString program2 = "uname";
        QStringList arguments3;
        arguments3 << "-v";
        p.start(program2, arguments3);
        p.waitForFinished();
        QString operatingsystem(p.readAllStandardOutput());
        operatingsystem.replace("\n", "");
        operatingsystem = operatingsystem.trimmed();

        QStringList arguments4;
        arguments4 << "-K";
        p.start(program2, arguments4);
        p.waitForFinished();
        QString kernelversion(p.readAllStandardOutput());
        kernelversion.replace("\n", "");
        kernelversion = kernelversion.trimmed();
        qDebug() << "kernelversion:" << kernelversion;

        QString program3 = "sysctl";
        QStringList arguments5;
        arguments5 << "-n" << "hw.model";
        p.start(program3, arguments5);
        p.waitForFinished();
        QString cpu(p.readAllStandardOutput());
        cpu = cpu.trimmed();
        cpu = cpu.replace("(R)", "®");
        cpu = cpu.replace("(TM)", "™");
        qDebug() << "cpu:" << cpu;

        QStringList arguments6;
        arguments6 << "-n" << "hw.realmem";
        p.start(program3, arguments6);
        p.waitForFinished();
        QString memory(p.readAllStandardOutput());
        memory = memory.trimmed();
        qDebug() << "memory:" << memory;
        double m = memory.toDouble();
        m = m/1024/1024/1024;
        qDebug() << "m:" << m;

        QStringList arguments7;
        arguments7 << "-n" << "kern.disks";
        p.start(program3, arguments7);
        p.waitForFinished();
        QString disks(p.readAllStandardOutput());
        disks = disks.replace("\n", "");
        QString disk = disks.split(" ")[0];
        qDebug() << "disk:" << disk;

        QString program4 = "lsblk";
        QStringList arguments8;
        arguments8 << disk;
        p.start(program4, arguments8);
        p.waitForFinished();
        QString diskinfo(p.readAllStandardOutput());
        QStringList di;
        di = diskinfo.split("\n");
        QString disksize ="Unknown";

        foreach (QString ds, di) {
            if(ds.startsWith(disk)) {
                // qDebug() << "ds:" << ds ;
                disksize = ds.simplified().split(" ")[2].trimmed().replace("G", " GiB"); // .simplified() replaces multiple spaces with one
                qDebug() << "disksize:" << disksize ;
            }
        }

        QString icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-hello.png";

        // If we found a way to read dmi without needing to be root, we could show a notebook icon for notebooks...
        // icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-laptop.png";

        // See https://github.com/openwebos/qt/blob/92fde5feca3d792dfd775348ca59127204ab4ac0/tools/qdbus/qdbusviewer/qdbusviewer.cpp#L477 for loading icon from resources
        QString helloSystemInfo;
        if(sha != "" && url != "" && build != "") {
            qDebug() << " xxxxxxxxxxxxxxxxxx  " ;
            helloSystemInfo = "</p>helloSystem build: "+ build +" for commit: <a href='" + url + "'>" + sha + "</a></p>";
        } else if(sha != "" && url != "") {
            helloSystemInfo = "</p>helloSystem commit: <a href='" + url + "'>" + sha + "</a></p>";
        }
        msgBox->setStandardButtons(QMessageBox::Close);
        // msgBox->setStandardButtons(0); // Remove button. FIXME: This makes it impossible to close the window; why?
        msgBox->setText("<center><img src=\"file://" + icon + "\"><h3>" + vendorname + " " + productname  + "</h3>" + \
                        "<p>" + operatingsystem +"</p><small>" + \
                        "<p>Kernel version: " + kernelversion +"</p>" + \
                        "<p>Processor: " + cpu +"<br>" + \
                        "Memory: " + QString::number(m) +" GiB<br>" + \
                        "Startup Disk: " + disksize +"</p>" + \
                        helloSystemInfo + \
                        "<p><a href='file:///COPYRIGHT'>FreeBSD copyright information</a><br>" + \
                        "Other components are subject to<br>their respective license terms</p>" + \
                        "</small></center>");
    }

    // Center window on screen
    msgBox->setFixedWidth(350); // FIXME: Remove hardcoding; but need to be able to center on screen
    msgBox->setFixedHeight(500); // FIXME: Same
    QRect screenGeometry = QGuiApplication::screens()[0]->geometry();
    int x = (screenGeometry.width()-msgBox->geometry().width()) / 2;
    int y = (screenGeometry.height()-msgBox->geometry().height()) / 2;
    msgBox->move(x, y);

    msgBox->setStyleSheet("QWidget { padding-right: 20px }"); // FIXME: Desperate attempt to get the text really centered

    msgBox->setModal(false);

    msgBox->show();

}

void AppMenuWidget::actionLaunch(QAction *action)
{
    qDebug() << "actionLaunch(QAction *action) called";
    // Setting a busy cursor in this way seems only to affect the own application's windows
    // rather than the full screen, which is why it is not suitable for this application
    // QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList pathToBeLaunched = {action->property("path").toString()};
    QProcess::startDetached("launch", pathToBeLaunched);
}

/*
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
*/


void AppMenuWidget::actionMinimizeAll()
{
    // TODO: In a similar way, implement "Hide <window name>" and "Hide others". For this we need to know the window ID of the frontmost application window
    qDebug() << "probono: KWindowSystem::activeWindow;" << "0x" + QString::number(KWindowSystem::activeWindow(), 16);
    // NOTE: This always prints the window ID of the menu itself, rather than the one of the otherwise frontmost application window
    // Hence we would need to store a variable somewhere that contains the window ID of the last non-menu window... or is there a better way?
    const auto &windows = KWindowSystem::windows();
    for (WId wid : windows) {
        KWindowSystem::minimizeWindow(wid);
    }
}

void AppMenuWidget::actionMaximizeAll()
{
    // TODO: In a similar way, implement "Hide <window name>" and "Hide others". For this we need to know the window ID of the frontmost application window
    qDebug() << "probono: KWindowSystem::activeWindow;" << "0x" + QString::number(KWindowSystem::activeWindow(), 16);
    // NOTE: This always prints the window ID of the menu itself, rather than the one of the otherwise frontmost application window
    // Hence we would need to store a variable somewhere that contains the window ID of the last non-menu window... or is there a better way?
    const auto &windows = KWindowSystem::windows();
    for (WId wid : windows) {
        KWindowSystem::activateWindow(wid);
    }
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

// Make it possible to click on the menu entry for a submenu
// https://github.com/helloSystem/Menu/issues/17
// Thanks Keshav Bhatt
// Alternative would be:
// https://stackoverflow.com/a/3799197 (seems to require subclassing QMenu; we cant to avoid subclassing Qt)
bool AppMenuWidget::eventFilter(QObject *watched, QEvent *event)
{
    if(event->type() == QEvent::MouseButtonRelease) // here we are checking mouse button press event
    {
        QMouseEvent *mouseEvent  = static_cast<QMouseEvent*>(event);
        QMenu *submenu = qobject_cast<QMenu*>(watched);  // Workaround for: no member named 'toolTip' in 'QObject'
        if(!submenu->rect().contains(mouseEvent->pos())) { // Prevent the Menu action from getting triggered when user click on actions in submenu
            // Gets executed when the submenu is clicked
            qDebug() << "Submenu clicked:" << submenu->property("path").toString();
            this->m_systemMenu->close(); // Could instead figure out the top-level menu iterating through submenu->parent();
            QString pathToBeLaunched = submenu->property("path").toString();
            if(QFile::exists(pathToBeLaunched)) {
                QProcess::startDetached("launch", {"Filer", pathToBeLaunched});
            } else {
                qDebug() << pathToBeLaunched << "does not exist";
            }
        }
    }
    return QWidget::eventFilter(watched,event);
}

