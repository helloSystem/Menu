    /*
 * Copyright (C) 2020 PandaOS Team.
 * Author:     rekols <revenmartin@gmail.com>
 * Portions Copyright (C) 2020-22 Simon Peter.
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
#include "mainwidget.h"
#include <QProcess>
#include <QHBoxLayout>
#include <QDebug>
#include <QMenu>
#include <QWidgetAction>
#include <QX11Info>
#include <QApplication>
#include <QAbstractItemView>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QList>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QPushButton>
#include <QStyle>
#include <QDesktopWidget>
#include <QScreen>
#include <QObject>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QMouseEvent>
#include <QTimer>
#include <QComboBox>
#include <QItemSelectionModel>
#include <QAbstractItemModel>
#include <QListView>
#include <QCryptographicHash>
#include <QWindow>
#include <QTimer>
#include <Baloo/Query>
#include <KF5/KWindowSystem/KWindowSystem>
#include <KF5/KWindowSystem/KWindowInfo>
#include <KF5/KWindowSystem/NETWM>

#if defined(Q_OS_FREEBSD)
#include <magic.h>
#include <sys/types.h>
#include <sys/extattr.h>
#endif

#include "mainwindow.h"
#include "thumbnails.h"

// SystemMenu is like QMenu but has a first menu item
// that changes depending on whether modifier keys are pressed
// https://stackoverflow.com/a/52756601
class SystemMenu: public QMenu {
private:
    QAction qCmdAbout;
    bool alt;

public:
    SystemMenu(QWidget *parent): QMenu(parent),
        qCmdAbout(tr("About This Computer")),
        alt(false)
    {
        connect(&qCmdAbout, SIGNAL(triggered()), this, SLOT(AppMenuWidget::actionAbout()));
        addAction(&qCmdAbout);
    }
protected:
    virtual void showEvent(QShowEvent *pQEvent) override
    {
        qDebug() << "SystemMenu::showEvent";
        update();
        QMenu::showEvent(pQEvent);
    }

    virtual void keyPressEvent(QKeyEvent *pQEvent) override
    {
        qDebug() << "SystemMenu::keyPressEvent";
        update(pQEvent->modifiers());
        QMenu::keyPressEvent(pQEvent);
    }
    virtual void keyReleaseEvent(QKeyEvent *pQEvent) override
    {
        qDebug() << "SystemMenu::keyReleaseEvent";
        update(pQEvent->modifiers());
        QMenu::keyReleaseEvent(pQEvent);
    }

private:
    void update()
    {
        update(
                    (QApplication::keyboardModifiers()
                     )
                    != 0);
    }
    void update(bool alt)
    {
        qDebug() << "alt:" << alt;
        if (!alt != !this->alt) {
            qCmdAbout.setText(alt ? tr("About helloDesktop") : tr("About This Computer"));
        }
        this->alt = alt;
    }
};

// Put cursor back in search box if user presses backspace while a menu item is highlighted
void AppMenuWidget::keyPressEvent(QKeyEvent * event) {
    if(event->key() == Qt::Key_Backspace) {
        searchLineEdit->setFocus();
        // FIXME: Wait until searchLineEdit has focus before continuing
   }
   QCoreApplication::sendEvent(parent(),event);
}

void SearchLineEdit::keyPressEvent(QKeyEvent * event) {
    if(event->key() == Qt::Key_Down) {

        emit editingFinished();
   } else {
    QCoreApplication::sendEvent(parent(),event);
   }
   QLineEdit::keyPressEvent(event);
}

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
            if (keyEvent->key() == (Qt::Key_Tab | Qt::Key_Alt))
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
            // reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText(tr("Search"));
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
            // reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText("");
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
        return;
    QStringList nameFilter({"*.app", "*.AppDir", "*.desktop", "*.AppImage", "*.appimage"});
    foreach (QString directory, locationsContainingApps) {
        // Shall we process this directory? Only if it contains at least one application, to optimize for speed
        // by not descending into directory trees that do not contain any applications at all. Can make
        // a big difference.

        QDir dir(directory);
        int numberOfAppsInDirectory = dir.entryList(nameFilter).length();
        QMenu *submenu;

        if(directory.endsWith(".app") == false && directory.endsWith(".AppDir") == false && numberOfAppsInDirectory > 0) {
            // qDebug() << "# Descending into" << directory;
            QStringList locationsToBeChecked = {directory};
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
                if(QFileInfo::exists(AppCand) == true) {
                    // qDebug() << "# Found" << AppCand;
                    QFileInfo fi(file.fileName());
                    QString base = fi.completeBaseName();  // The name of the .app directory without suffix // baseName() gets it wrong e.g., when there are dots in version numbers
                    QAction *action = submenu->addAction(base);
                    action->setToolTip(file.absoluteFilePath());
                    action->setProperty("path", file.absoluteFilePath());
                    QString IconCand = QDir(candidate).canonicalPath() + "/Resources/" + nameWithoutSuffix + ".png";
                    if(QFileInfo::exists(IconCand) == true) {
                        // qDebug() << "#   Found icon" << IconCand;
                        action->setIcon(QIcon(IconCand));
                        action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus
                    }
                }
            }
            else if (file.fileName().endsWith(".AppDir")) {
                QString AppCand = QDir(candidate).canonicalPath() + "/" + "AppRun";
                // qDebug() << "################### Checking" << AppCand;
                if(QFileInfo::exists(AppCand) == true){
                    // qDebug() << "# Found" << AppCand;
                    QFileInfo fi(file.fileName());
                    QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers
                    QStringList executableAndArgs = {AppCand};
                    QAction *action = submenu->addAction(base);
                    action->setToolTip(file.absoluteFilePath());
                    action->setProperty("path", file.absoluteFilePath());
                    QString IconCand = QDir(candidate).canonicalPath() + "/.DirIcon";
                    if(QFileInfo::exists(IconCand) == true) {
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
                action->setDisabled(true); // As a reminder that we consider those legacy and encourage people to swtich
                // Finding the icon file is much more involved with XDG than with our simplified .app bundles, so it is not implemented here
            }
            else if (file.fileName().endsWith(".AppImage") || file.fileName().endsWith(".appimage")) {
                // .desktop file
                // qDebug() << "# Found" << file.fileName();
                QFileInfo fi(file.fileName());
                QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots in version numbers
                QStringList executableAndArgs = {fi.absoluteFilePath()};
                QAction *action = submenu->addAction(base);
                action->setProperty("path", file.absoluteFilePath());
                QString IconCand = Thumbnail(QDir(candidate).absolutePath(), QCryptographicHash::Md5,Thumbnail::ThumbnailSizeNormal, nullptr).getIconPath();
                // qDebug() << "#   ############################### thumbnail for" << QDir(candidate).absolutePath();
                if(QFileInfo::exists(IconCand) == true) {
                    // qDebug() << "#   Found thumbnail" << IconCand;
                    action->setIcon(QIcon(IconCand));
                    action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus
                } else {
                    // TODO: Request thumbnail; https://github.com/KDE/kio-extras/blob/master/thumbnail/thumbnail.cpp
                    // qDebug() << "#   Did not find thumbnail" << IconCand << "TODO: Request it from thumbnailer";
                }
            }
            else if (locationsContainingApps.contains(candidate) == false && file.isDir() && candidate.endsWith("/..") == false && candidate.endsWith("/.") == false && candidate.endsWith(".app") == false && candidate.endsWith(".AppDir") == false) {
                // qDebug() << "# Found" << file.fileName() << ", a directory that is not an .app bundle nor an .AppDir";
                QStringList locationsToBeChecked({candidate});
                findAppsInside(locationsToBeChecked, m_systemMenu, watcher);
            }
        }
    }
}
void iterate(const QModelIndex & index, const QAbstractItemModel * model,
             const std::function<int(const QModelIndex&, int depth)>  & fun,
             int depth=0)
{

    if (index.isValid())
            if(fun(index, depth)>0)
            return;
    if ((index.flags() & Qt::ItemNeverHasChildren) || !model->hasChildren(index)) return;
    auto rows = model->rowCount(index);
    auto cols = model->columnCount(index);
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            iterate(model->index(i, j, index), model, fun, depth+1);
}
AppMenuWidget::AppMenuWidget(QWidget *parent)
    : QWidget(parent),
      m_typingTimer(new QTimer(this))
{
    // probono: Reload menu when something changed in a watched directory; FIXME: This is not functional yet
    // https://github.com/helloSystem/Menu/issues/15
    watcher = new QFileSystemWatcher(this);
    // watcher->connect(watcher, SIGNAL(directoryChanged(QString)), this, SLOT(updateMenu())); // We need a slot that rebuilds the menu
    connect(watcher, SIGNAL(directoryChanged(QString)), SLOT(rebuildMenu()));                // We need a slot that rebuilds the menu

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setAlignment(Qt::AlignCenter); // Center QHBoxLayout vertically
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);

    // Add search box to menu
    searchLineEdit = new SearchLineEdit(this);

    // Make sure the search box gets cleared when this application loses focus
    searchLineEdit->setObjectName("actionSearch"); // probono: This name can be used in qss to style it specifically
    searchLineEdit->setFixedHeight(22); // FIXME: Dynamically get the height of a QMenuItem and use that
    searchLineEdit->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false);
    searchLineEdit->setFocus();
    m_searchMenu = new QMenu();
    m_searchMenu->setIcon(QIcon::fromTheme("search-symbolic"));
    std::function<int(QModelIndex idx,int depth)> traverse =[this](QModelIndex idx,int depth) {
QAction * action =idx.data().value<QAction*>();
if(action->isVisible() && idx.parent().isValid()) {
    m_wasVisible.push_back(cmpAction({idx.parent().data().value<QAction*>()->text().toStdString(),idx.data().value<QAction*>()->text().toStdString(),idx.row()}));


}
if(action->menu()) {
            Q_EMIT action->menu()->aboutToShow();

    }
    return 0;
    };
    std::function<int(QModelIndex idx,int depth)> traverse1 =[this](QModelIndex idx,int depth) {
    QAction * action =idx.data().value<QAction*>();
    if(action->menu()) {
            Q_EMIT action->menu()->aboutToShow();

    }
    return 0;
    };
    connect(m_searchMenu,&QMenu::aboutToShow
            ,[this,traverse1]() {

iterate(QModelIndex(),m_appMenuModel,traverse1);
        searchLineEdit->setFocus();
    });

    // https://github.co    m/helloSystem/Menu/issues/95
/*
    connect(qApp, &QApplication::focusWindowChanged, this, [this](QWindow *w) {
        if(w) {
            qDebug() << __LINE__ << w<< qApp->focusWindow();

        }
        if (!w) { // This is not always true and does always work but never on qt  5.12.8 on ubuntu 20.04 with lxqt //
qDebug() << __LINE__ << w<< qApp->focusWindow();



        }
            });
    */
    setFocusPolicy(Qt::NoFocus);
    // Prepare System menu
    m_systemMenu = new SystemMenu(this); // Using our SystemMenu subclass instead of a QMenu to be able to toggle "About..." when modifier key is pressed
    m_systemMenu->setTitle(tr("System"));
    QWidgetAction *widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(searchLineEdit);
    m_searchMenu->addAction(widgetAction);

    connect(searchLineEdit,&QLineEdit::editingFinished,this,&AppMenuWidget::searchEditingDone);

    // connect(searchLineEdit,&QLineEdit::textChanged,this,&AppMenuWidget::searchMenu);
    // Do not do this immediately, but rather delayed
    // https://wiki.qt.io/Delay_action_to_wait_for_user_interaction
    m_typingTimer->setSingleShot(true); // Ensure the timer will fire only once after it was started
    connect(m_typingTimer, &QTimer::timeout, this, &AppMenuWidget::searchMenu);
    connect(searchLineEdit,&QLineEdit::textChanged,this,&AppMenuWidget::refreshTimer);

    m_systemMenu->setToolTipsVisible(true); // Works; shows the full path

    // If we were using a QMenu, we would do:
    // QAction *aboutAction = m_systemMenu->addAction(tr("About This Computer"));
    // connect(aboutAction, SIGNAL(triggered()), this, SLOT(actionAbout()));
    // Since we are using our SystemMenu subclass instead which already contains the first menu item, we do:
    connect(m_systemMenu->actions().constFirst(), SIGNAL(triggered()), this, SLOT(actionAbout()));

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
    QAction *forceQuitAction = m_systemMenu->addAction(tr("Force Quit Application"));
    connect(forceQuitAction, SIGNAL(triggered()), this, SLOT(actionForceQuit()));
    forceQuitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Escape));
    m_systemMenu->addSeparator();
    QAction *restartAction = m_systemMenu->addAction(tr("Restart"));
    connect(restartAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    QAction *logoutAction = m_systemMenu->addAction(tr("Log Out"));
    connect(logoutAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    QAction *shutdownAction = m_systemMenu->addAction(tr("Shut Down"));
    connect(shutdownAction, SIGNAL(triggered()), this, SLOT(actionLogout()));
    // Add main menu
    m_menuBar = new QMenuBar(this);
    m_menuBar->setContentsMargins(0, 0, 0, 0);

    integrateSystemMenu(m_menuBar); // Add System menu to main menu
    layout->addWidget(m_menuBar, 0, Qt::AlignLeft);
    layout->insertStretch(2); // Stretch after the main menu, which is the 2nd item in the layout

    m_appMenuModel = new AppMenuModel(m_menuBar);


  connect(m_appMenuModel,&AppMenuModel::menuImported,this,[this,traverse]{
m_wasVisible.clear();
        qDebug() <<__LINE__ << m_appMenuModel->menuAvailable();
            if(m_appMenuModel->menuAvailable()) {

                qDebug() <<__LINE__ << m_appMenuModel->menuAvailable();
                m_appMenuModel->menu()->aboutToShow();
iterate(QModelIndex(),m_appMenuModel,traverse);


    }
    });


    connect(m_appMenuModel, &AppMenuModel::menuAvailableChanged, this, &AppMenuWidget::updateMenu);

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppMenuWidget::delayUpdateActiveWindow);
    connect(KWindowSystem::self(), static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(&KWindowSystem::windowChanged),
            this, &AppMenuWidget::onWindowChanged);

    // Load action search
    actionCompleter = nullptr;
    MenuImporter *menuImporter = new MenuImporter(this);
    menuImporter->connectToBus();

}

void AppMenuWidget::searchEditingDone() {
     if(m_searchMenu && m_searchMenu->actions().count()>1) {
        searchLineEdit->clearFocus();
        for(QAction *findActivateeCanidcate : m_searchMenu->actions())
            if(!findActivateeCanidcate->isSeparator()) {
                m_searchMenu->setActiveAction(findActivateeCanidcate);
                break;
            }
            }
}

void AppMenuWidget::refreshTimer() {
    qDebug() << __LINE__;
    m_typingTimer->start(300); // https://wiki.qt.io/Delay_action_to_wait_for_user_interaction
}

void AppMenuWidget::focusMenu() {
    QMouseEvent event(QEvent::MouseButtonPress,QPoint(0,0),
    m_menuBar->mapToGlobal(QPoint(0,0)),Qt::LeftButton,0,0);
    QApplication::sendEvent(m_menuBar,&event);
    searchLineEdit->setFocus();
}
AppMenuWidget::~AppMenuWidget() {
}

void AppMenuWidget::integrateSystemMenu(QMenuBar *menuBar) {
    if(!menuBar || !m_systemMenu)
        return;

    m_searchMenu->setToolTipsVisible(true);
    menuBar->addMenu(m_searchMenu);

    menuBar->addMenu(m_systemMenu);

}
/*
void AppMenuWidget::handleActivated(const QString &name) {
    m_appMenuModel->execute(name);
    searchLineEdit->clear();
    m_searchMenu->close();
}*/

void AppMenuWidget::updateActionSearch() {


    /*
    /// Update the action search.
    actionSearch->clear();
    actionSearch->update(menuBar);

    /// Update completer
    if(actionCompleter) {
        actionCompleter->deleteLater();
            }
        actionCompleter = new QCompleter(m_appMenuModel,this);
        connect(actionCompleter,
                QOverload<const QString &>::of(&QCompleter::activated),
                this,
                &AppMenuWidget::handleActivated);

    actionCompleter->setCompletionColumn(0);
    actionCompleter->setCompletionRole(Qt::UserRole+2);
    actionCompleter->setCompletionMode(QCompleter::PopupCompletion);
    // TODO: https://stackoverflow.com/a/33790639
    // We could customize more aspects of the list view of the completer by
    //setting the CompletionMode to InlineCompletion, so there will be no popup.
    // Then make your QListView indepedant of the QLineEdit;
    // just react to signals that indicate when a view types some text,...

    KWindowSystem::setType(actionCompleter->popup()->winId(), NET::DropdownMenu);

    //actionCompleter->popup()->setObjectName("actionCompleterPopup");
    // static_cast<QListView *>(actionCompleter->popup())->setSpacing(10);
    // static_cast<QListView *>(actionCompleter->popup())->setUniformItemSizes(true);
    // static_cast<QListView *>(actionCompleter->popup())->setContentsMargins(10,10,0,10); // FIXME: Does not seem to work, why?

    // Empty search field on selection of an item, https://stackoverflow.com/a/11905995

    //QObject::connect(actionCompleter, SIGNAL(activated(const QString&)),
      //               searchLineEdit, SLOT(clear()),
        //             Qt::QueuedConnection);
*/
    // Make more than 7 items visible at once
    // actionCompleter->setMaxVisibleItems(35);

    // compute needed width
    // const QAbstractItemView * popup = actionCompleter->popup();
    //actionCompleter->popup()->setMinimumWidth(350);
    //actionCompleter->popup()->setMinimumWidth(600);

    //actionCompleter->popup()->setContentsMargins(100,100,100,100);

    // Make the completer match search terms in the middle rather than just those at the beginning of the menu
    //actionCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    //actionCompleter->setFilterMode(Qt::MatchContains);

    // Set first result active; https://stackoverflow.com/q/17782277. FIXME: This does not work yet. Why?
    //QItemSelectionModel* sm = new QItemSelectionModel(actionCompleter->completionModel());
    //actionCompleter->popup()->setSelectionModel(sm);
    //sm->select(actionCompleter->completionModel()->index(0,0), QItemSelectionModel::Select);

    //auto* flt = new AutoSelectFirstFilter(searchLineEdit);
    //actionCompleter->popup()->installEventFilter(flt);

    //actionCompleter->popup()->setAlternatingRowColors(false);
    // actionCompleter->popup()->setStyleSheet("QListView::item { color: green; }"); // FIXME: Does not work. Why?
    //searchLineEdit->setCompleter(actionCompleter);
    // Sort results of the Action Search
    //actionCompleter->completionModel()->sort(0,Qt::SortOrder::AscendingOrder);


}

void AppMenuWidget::searchMenu() {
    QString searchString = searchLineEdit->text();
    qDebug() << searchString << __LINE__;
    for(QAction *sr: qAsConst(searchResults)) {
        if(m_searchMenu->actions().contains(sr)) {
            m_searchMenu->removeAction(sr);
            CloneAction * ca = qobject_cast<CloneAction*>(sr);
            if(ca) {
                ca->resetOrigShortcutContext();
                ca->disconnectOnClear();
            }
        }
    }


                std::function<int(QModelIndex idx,int depth)> setResultVisbileMbar =[this,searchString](QModelIndex idx,int depth) {
                    QAction * action =idx.data().value<QAction*>();
                    if(searchString.isEmpty()) {
                        qDebug() <<__LINE__<< m_wasVisible.size();

                            if(idx.parent().isValid()) {
                                std::vector<cmpAction>::iterator it = std::find(m_wasVisible.begin(),m_wasVisible.end(),cmpAction({idx.parent().data().value<QAction*>()->text().toStdString(),idx.data().value<QAction*>()->text().toStdString(),idx.row()}));
                                bool visible  = it != m_wasVisible.end();
                                action->setVisible(visible);
                                qDebug() << visible << __LINE__ << action->text();

                                QModelIndex p = idx.parent();
                                while(p.isValid()) {

                                       p.data().value<QAction*>()->setVisible(true);


                                 p= p.parent();
                                }

                            }
                            return 0;
                            }


                bool visible = false;
               if(idx.parent().isValid()) {
                cmpAction cmp1 ={
                    idx.parent().data().value<QAction*>()->text().toStdString(),
                            idx.data().value<QAction*>()->text().toStdString(),
                            idx.row()
                };
                std::vector<cmpAction>::iterator it= std::find(m_wasVisible.begin(),m_wasVisible.end(),cmp1);
                visible = it!=m_wasVisible.end();
                }
                 if(!searchString.isEmpty() && action->text().contains(searchString,Qt::CaseInsensitive)) {


                    qDebug()  << visible << __LINE__;
                 action->setVisible(true && visible);


                 QModelIndex p = idx.parent();
                 QStringList names;
                 while(p.isValid()) {

                        p.data().value<QAction*>()->setVisible(true);

                        names << p.data().value<QAction*>()->text();
                        p= p.parent();
                 }
                    std::reverse(names.begin(),names.end());

                    QAction *orig= idx.data().value<QAction*>();
                    if(!orig->menu()) {
                    CloneAction *cpy = new CloneAction(orig);
                    cpy->setText(names.join(" → ") + " → " + orig->text());
                    cpy->setShortcut(orig->shortcut());
                    cpy->setToolTip(orig->toolTip());
                    cpy->updateMe();
                    cpy->setShortcutContext(Qt::ApplicationShortcut);
                    orig->setShortcutContext(Qt::WindowShortcut);
                    searchResults << cpy;
                    connect(cpy,&QAction::triggered,this,[this]{
                        searchLineEdit->setText("");
                        searchLineEdit->textChanged("");
                        m_searchMenu->close();

                    });
                    cpy->setDisconnectOnClear(connect(orig,&QAction::triggered,this,[this]{
                        searchLineEdit->setText("");
                        searchLineEdit->textChanged("");
                        m_searchMenu->close();

                    }));
                    m_searchMenu->addAction(cpy);
                    }


                 }


                  else if(!searchString.isEmpty()) {
                         action->setVisible(false);

                }

                 return 0;
                };
        m_searchMenu->addSeparator();



        iterate(QModelIndex(),m_appMenuModel,setResultVisbileMbar);
             m_isSearching=false;



    m_searchMenu->addSeparator();



    QList<QMenu*> menus;
    menus << m_systemMenu;
    QStringList names;

    for(QMenu * menu : qAsConst(menus)) {

        m_appMenuModel->filterMenu(menu,searchString,searchString=="",names);


    }
const QStringList keys = m_appMenuModel->filteredActions().keys();
for(const QString &v : keys) {
    QAction *orig = m_appMenuModel->filteredActions()[v];
    CloneAction *cpy = new CloneAction(orig);
    cpy->setText(v);
    cpy->setShortcut(orig->shortcut());
    cpy->setToolTip(orig->toolTip());
    cpy->updateMe();
    cpy->setShortcutContext(Qt::ApplicationShortcut);
    orig->setShortcutContext(Qt::WindowShortcut);
    searchResults << cpy;
    connect(cpy,&QAction::triggered,this,[this]{
        searchLineEdit->setText("");
        emit searchLineEdit->textChanged("");
        m_searchMenu->close();

    });
    cpy->setDisconnectOnClear(connect(orig,&QAction::triggered,this,[this]{
        searchLineEdit->setText("");
        emit searchLineEdit->textChanged("");
        m_searchMenu->close();

    }));
    m_searchMenu->addAction(cpy);
    }


// probono: Use Baloo API and add baloo search results to the Search menu; see below for a rudimentary non-API version
QMimeDatabase mimeDatabase;
if(searchString != "") {
    // Prevent separator from ever being directly underneath the search box, because this breaks arrow key nagivation
    if(m_appMenuModel->filteredActions().count()>0)
        searchResults << m_searchMenu->addSeparator(); // The items in searchResults get removed when search results change
    Baloo::Query query;
    query.setSearchString(searchString);
    query.setLimit(21);
    Baloo::ResultIterator iter = query.exec();
    int i=0;
    bool showMore = false;
    while (iter.next()) {
        i++;
        if(i == query.limit()) {
            showMore = true;
            break;
        }
        QMimeType mimeType;
        mimeType = mimeDatabase.mimeTypeForFile(QFileInfo(iter.filePath()));
        QAction *res = new QAction();
        res->setText(iter.filePath().split("/").last());
        res->setToolTip(iter.filePath());


        // If there is a thumbnail, show it
        QString IconCand = Thumbnail(QDir(iter.filePath()).absolutePath(), QCryptographicHash::Md5,Thumbnail::ThumbnailSizeNormal, nullptr).getIconPath();
        // qDebug() << "#   ############################### thumbnail for" << QDir(iter.filePath()).absolutePath();
        if(QFileInfo::exists(IconCand) == true) {
            // qDebug() << "#   Found thumbnail" << IconCand;
            res->setIcon(QIcon(IconCand));
        } else {
            // TODO: Request thumbnail; https://github.com/KDE/kio-extras/blob/master/thumbnail/thumbnail.cpp
            // qDebug() << "#   Did not find thumbnail" << IconCand << "TODO: Request it from thumbnailer";
            QIcon icon = QIcon::fromTheme(mimeType.iconName());
            res->setIcon(icon);
        }

        res->setIconVisibleInMenu(true);
        res->setProperty("path", iter.filePath());
        connect(res,&QAction::triggered,this,[this, res]{
            openBalooSearchResult(res);
            searchLineEdit->setText("");
            emit searchLineEdit->textChanged("");
            m_searchMenu->close();
        });

        m_searchMenu->addAction(res);
        searchResults << res; // The items in searchResults get removed when search results change
     }

    if(showMore) {
        QAction *a = new QAction();
        searchResults << a; // The items in searchResults get removed when search results change
        a->setText("...");
        a->setDisabled(true);
        m_searchMenu->addAction(a);
    }
}

// probono: query baloosearch and add baloo search results to the Search menu; see above for an API version
#if 0
    QProcess p;
    QString program = "baloosearch";
    QStringList arguments;
    arguments << "-l" << "20" << searchString;
    p.start(program, arguments);
    p.waitForFinished();
    QStringList balooSearchResults;
    QString result(p.readAllStandardOutput());
    if(result != ""){
        // m_searchMenu->addSeparator(); // FIXME: Would be nicer but breaks autoselection when only one search result is left
        balooSearchResults = result.split('\n');
        for(QString searchResult: balooSearchResults){
            if(! searchResult.startsWith("/")) {
                continue;
            }
            qDebug() << "probono: searchResult:" << searchResult;
            QAction *orig = new QAction();
            CloneAction *a = new CloneAction(orig); // Using a CloneAction so that these search results get removed like menu search results; FIXME: Do more efficiently
            a->setToolTip(searchResult);
            a->setText(searchResult.split("/").last());
            searchResults << a; // The items in searchResults get removed when search results change
            connect(a,&QAction::triggered,this,[this, a]{
                openBalooSearchResult(a);
                searchLineEdit->setText("");
                searchLineEdit->textChanged("");
                m_searchMenu->close();
            });
            a->setDisconnectOnClear(connect(orig,&QAction::triggered,this,[this]{
                searchLineEdit->setText("");
                searchLineEdit->textChanged("");
                m_searchMenu->close();
            }));
            a->setIcon(QIcon::fromTheme("document").pixmap(24, 24));
            a->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets Qt::AA_DontShowIconsInMenus
            a->setProperty("path", searchResult);
            m_searchMenu->addAction(a);
        }
        if(balooSearchResults.length()>20) {
            CloneAction *a = new CloneAction(new QAction()); // Using a CloneAction so that these search results get removed like menu search results; FIXME: Do more efficiently
            searchResults << a; // The items in searchResults get removed when search results change
            a->setText("...");
            a->setDisabled(true);
            m_searchMenu->addAction(a);
        }
    }
}
#endif

// If there is only one search result, select it
int number_of_enabled_actions = 0;
const QList<QAction*> actions = m_searchMenu->actions();
for (QAction *a : actions) {
    if(a->isEnabled())
            number_of_enabled_actions++;
}
// qDebug() << "probono: number_of_enabled_actions" << number_of_enabled_actions;
// QUESITON: Unclear whether it is 2 or 3, depending on whether one menu action or one Baloo search result is there...
if(number_of_enabled_actions == 2 || ( number_of_enabled_actions == 3 && m_appMenuModel->filteredActions().count()==1)) {
    searchEditingDone();
} else {
    auto evt = new QMouseEvent(QEvent::MouseMove, m_menuBar->actionGeometry(m_menuBar->actions().at(0)).center(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::postEvent(m_menuBar, evt);
}

m_appMenuModel->clearFilteredActions();


}

void AppMenuWidget::rebuildMenu()
{
    qobject_cast<MainWidget*>(parent())->rebuildSystemMenu();
    qDebug() << "AppMenuWidget::rebuildMenu() called";
}

//doesn't work for https://github.com/helloSystem/Menu/issues/16
//what does this even do??
void AppMenuWidget::updateMenu() {
qDebug() <<__LINE__ <<":" <<__FILE__;
if(!m_appMenuModel->menuAvailable()) {
    int cnt = m_menuBar->actions().count();
    QList<QAction*> remove;
    for(int i=2;i<cnt;i++)
         remove.append(m_menuBar->actions().at(i));
    for(QAction *r : remove) {
        m_menuBar->removeAction(r);

   }


m_appMenuModel->invalidateMenu();
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
    /*
    if (e->type() == QEvent::ApplicationFontChange) {
        QMenu *menu = m_appMenuModel->menu();
        if (menu) {
            const QList<QAction*> actions = menu->actions();
            for (QAction *a : actions) {
                a->setFont(qApp->font());
            }
        }
        m_menuBar->setFont(qApp->font());
        m_menuBar->update();
    }
*/
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

    if (m_currentWindowID != KWindowSystem::activeWindow()) {
        qobject_cast<MainWindow*>(this->parent()->parent())->hideApplicationName();
        // TODO: Need to trigger updating the menu here? Sometimes it stays blank after an application has been closed
    }

    if(m_currentWindowID >0 && m_currentWindowID != m_windowID && m_windowID !=0) {

        //searchLineEdit->clear();
        //searchLineEdit->textChanged("");

        // bool isMax = info.hasState(NET::Max);
    }

    m_currentWindowID = m_windowID;
}

void AppMenuWidget::onWindowChanged(WId /*id*/, NET::Properties /*properties*/, NET::Properties2 /*properties2*/)
{
    if (m_windowID && m_windowID== KWindowSystem::activeWindow())
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

    QMessageBox *msgBox = new QMessageBox(this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    if (QApplication::keyboardModifiers()){

        msgBox->setWindowTitle(tr("About helloDesktop"));

        QString icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-hello.png";

        // If we found a way to read dmi without needing to be root, we could show a notebook icon for notebooks...
        // icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-laptop.png";

        msgBox->setStandardButtons(QMessageBox::Close);

        msgBox->setText("<center><img src=\"file://" + icon + "\"><h3>helloDesktop</h3>" + \
                        "<p>Lovingly crafted by true connoisseurs<br>of the desktop metaphor</p>" + \
                        "<p>Inspired by the timeless vision<br>of Bill Atkinson and Andy Hertzfeld</p>" + \
                        "<small>" + \
                        "<p>Recommended reading: <a href='https://dl.acm.org/doi/book/10.5555/573097'>ISBN 978-0-201-2216-4</a><br>" + \
                        "</small></center>");

        // Center window on screen
        msgBox->setFixedWidth(350); // FIXME: Remove hardcoding; but need to be able to center on screen
        msgBox->setFixedHeight(500); // FIXME: Same
        QRect screenGeometry = QGuiApplication::screens().constFirst()->geometry();
        int x = (screenGeometry.width()-msgBox->geometry().width()) / 2;
        int y = (screenGeometry.height()-msgBox->geometry().height()) / 2;
        msgBox->move(x, y);

        msgBox->setStyleSheet("QWidget { padding-right: 20px }"); // FIXME: Desperate attempt to get the text really centered

        msgBox->setModal(false);

        msgBox->show();
        return;

    } else {

        msgBox->setWindowTitle(tr("About This Computer"));

        QString url;
        QString sha;
        QString build;

#if defined(Q_OS_FREEBSD)
        // Try to get extended attributes on the /.url file
        // TODO: We should use our cross-platform funcitons for this; we are using them
        // in other places already in Menu. That way we can remove the ifdef
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


#if defined(Q_OS_FREEBSD)
            // On FreeBSD, get information about the machine
            QProcess p;
            p.setProgram("kenv");
            p.setArguments({"-q", "smbios.system.maker"});
            p.start();
            p.waitForFinished();
            QString vendorname(p.readAllStandardOutput());
            vendorname.replace("\n", "");
            vendorname = vendorname.trimmed();
            qDebug() << "vendorname:" << vendorname;

            p.setArguments({"-q", "smbios.system.product"});
            p.start();
            p.waitForFinished();
            QString productname(p.readAllStandardOutput());
            productname.replace("\n", "");
            productname = productname.trimmed();
            qDebug() << "systemname:" << productname;
            msgBox->setText("<b>" + vendorname + " " + productname + "</b>");

            p.setProgram("pkg");
            p.setArguments({"info", "hello"});
            p.start();
            p.waitForFinished();
            QString operatingsystem(p.readAllStandardOutput());
            operatingsystem = operatingsystem.split("\n")[0].trimmed();
            if(operatingsystem != "") {
                // We are running on helloSystem
                operatingsystem = operatingsystem.replace("hello-", "helloSystem ").replace("_", " (Build ") + ")";
            } else {
                // We are not running on helloSystem (e.g., on FreeBSD + helloDesktop)
                operatingsystem = "helloDesktop (not running on helloSystem)";
            }

            p.setProgram("sysctl");
            p.setArguments({"-n", "hw.model"});
            p.start();
            p.waitForFinished();
            QString cpu(p.readAllStandardOutput());
            cpu = cpu.trimmed();
            cpu = cpu.replace("(R)", "®");
            cpu = cpu.replace("(TM)", "™");
            qDebug() << "cpu:" << cpu;

            p.setArguments({"-n", "hw.realmem"});
            p.start();
            p.waitForFinished();
            QString memory(p.readAllStandardOutput().trimmed());
            qDebug() << "memory:" << memory;
            double m = memory.toDouble();
            m = m/1024/1024/1024;
            qDebug() << "m:" << m;

            p.setProgram("freebsd-version");
            p.setArguments({"-k"});
            p.start();
            p.waitForFinished();
            QString kernelVersion(p.readAllStandardOutput());

            p.setArguments({"-u"});
            p.start();
            p.waitForFinished();
            QString userlandVersion(p.readAllStandardOutput());

            QString icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-hello.png";

            // If we found a way to read dmi without needing to be root, we could show a notebook icon for notebooks...
            // icon = "/usr/local/share/icons/elementary-xfce/devices/128/computer-laptop.png";

            // See https://github.com/openwebos/qt/blob/92fde5feca3d792dfd775348ca59127204ab4ac0/tools/qdbus/qdbusviewer/qdbusviewer.cpp#L477 for loading icon from resources
            QString helloSystemInfo;
            if(sha != "" && url != "" && build != "") {
                helloSystemInfo = "</p>helloSystem build: "+ build +" for commit: <a href='" + url + "'>" + sha + "</a></p>";
            } else if(sha != "" && url != "") {
                helloSystemInfo = "</p>helloSystem commit: <a href='" + url + "'>" + sha + "</a></p>";
            }
            msgBox->setStandardButtons(QMessageBox::Close);
            // msgBox->setStandardButtons(0); // Remove button. FIXME: This makes it impossible to close the window; why?
            msgBox->setText("<center><img src=\"file://" + icon + "\"><h3>" + vendorname + " " + productname  + "</h3>" + \
                            "<p>" + operatingsystem +"</p><small>" + \
                            "<p>FreeBSD kernel version: " + kernelVersion +"<br>" + \
                            "FreeBSD userland version: " + userlandVersion + "</p>" + \
                            "<p>Processor: " + cpu +"<br>" + \
                            "Memory: " + QString::number(m) +" GiB<br>" + \
                            helloSystemInfo + \
                            "<p><a href='file:///COPYRIGHT'>FreeBSD copyright information</a><br>" + \
                            "Other components are subject to<br>their respective license terms</p>" + \
                            "</small></center>");

#else
        msgBox->setText(QString("<center><h3>helloDesktop</h3>"
                        "<p>Running on an unsupported operating system<br>"
                        "with reduced functionality</p>"
                        "<small><p>The full desktop experience<br>"
                        "can best be experienced on helloSystem<br>"
                        "which helloDesktop is designed for</p>"
                        ""
                        "<a href='https://hellosystem.github.io'>https://hellosystem.github.io/</a><br>"
                        "</small></center>"));

#endif

        // Center window on screen
        // msgBox->setFixedWidth(350); // FIXME: Remove hardcoding; but need to be able to center on screen
        // msgBox->setFixedHeight(500); // FIXME: Same
        QRect screenGeometry = QGuiApplication::screens().constFirst()->geometry();
        int x = (screenGeometry.width()-msgBox->geometry().width()) / 2;
        int y = (screenGeometry.height()-msgBox->geometry().height()) / 2;
        msgBox->move(x, y);

        msgBox->setStyleSheet("QWidget { padding-right: 20px }"); // FIXME: Desperate attempt to get the text really centered

        msgBox->setModal(false);

        msgBox->show();
    }
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

// probono: When a modifier key is held down, then just show the item in Filer;
// otherwise launch it if it is an application bundle, or open it if it is not
void AppMenuWidget::openBalooSearchResult(QAction *action)
{
    QString pathToBeLaunched = action->property("path").toString();
    // TODO: Maybe just show the file in the file manager when a modifier key is pressed?
    QProcess p;
    if (QApplication::keyboardModifiers()){
        p.setProgram("gdbus");
        // probono: Am I the only one who finds the next line utterly overcomplicated to "tell Filer to show pathToBeLaunched"?
        p.setArguments({ "call", "--session", "--dest", "org.freedesktop.FileManager1", "--object-path", "/org/freedesktop/FileManager1", "--method", "org.freedesktop.FileManager1.ShowItems", "[\"file:///" + pathToBeLaunched + "\"]", "\"\"" });
        p.startDetached();
    } else {
        p.setArguments({pathToBeLaunched});
        if(pathToBeLaunched.endsWith(".app") || pathToBeLaunched.endsWith(".AppDir") || pathToBeLaunched.endsWith(".AppImage")) {
            p.setProgram("launch");
            p.startDetached();
        } else {
            p.setProgram("open");
            p.startDetached();
        }
    }
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
    // Hence we would need to store a variable somewhere that contains the window ID of the last non-menu window... or is there a btter way?
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
    // Hence we would need to store a variable somewhere that contains the window ID of the last non-menu window... or is there a btter way?
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
        QProcess *p = new QProcess();
        p->setProgram(QCoreApplication::applicationDirPath() + QString("/Shutdown"));
        p->startDetached();
    } else {
        qDebug() << "Shutdown executable not available next to Menubar executable, exiting";
        QApplication::exit(); // In case we are lacking the Shutdown executable
    }
}

void AppMenuWidget::actionForceQuit()
{
    qDebug() << "actionForceQuit() called";
    QProcess *p = new QProcess();
    p->setProgram("xkill");
    p->startDetached();
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
        qDebug() << __FILE__ ":"<<__LINE__;
        QMouseEvent *mouseEvent  = static_cast<QMouseEvent*>(event);
        QMenu *submenu = qobject_cast<QMenu*>(watched);  // Workaround for: no member named 'toolTip' in 'QObject'
        if(!submenu->rect().contains(mouseEvent->pos())) { // Prevent the Menu action from getting triggred when user click on actions in submenu
            // Gets executed when the submenu is clicked
            qDebug() << "Submenu clicked:" << submenu->property("path").toString();
            this->m_systemMenu->close(); // Could instead figure out the top-level menu iterating through submenu->parent();
            QString pathToBeLaunched = submenu->property("path").toString();
            if(QFile::exists(pathToBeLaunched)) {
                QProcess::startDetached("open", {pathToBeLaunched});
            } else {
                qDebug() << pathToBeLaunched << "does not exist";
            }
        }
    }
    return QWidget::eventFilter(watched,event);
}
