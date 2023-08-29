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
#include "menuqcalc.h"
#include <chrono>
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
#include <QLabel>
#include <QList>
#include <QDBusServiceWatcher>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QDesktopWidget>
#include <QScreen>
#include <QObject>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QCompleter>
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
#include <kglobalaccel.h>
#include <cmath>
#include <QClipboard>
#include <QUrl>

#if defined(Q_OS_FREEBSD)
#  include <magic.h>
#  include <sys/types.h>
#  include <sys/extattr.h>
#endif

#include <QFileSystemModel>
#include <QKeySequence>
#include <signal.h>

#include "mainwindow.h"
#include "thumbnails.h"

// SystemMenu is like QMenu but has a first menu item
// that changes depending on whether modifier keys are pressed
// https://stackoverflow.com/a/52756601
class SystemMenu : public QMenu
{
private:
    QAction qCmdAbout;
    bool alt;

public:
    SystemMenu(QWidget *parent) : QMenu(parent), qCmdAbout(tr("About This Computer")), alt(false)
    {
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
    void update() { update((QApplication::keyboardModifiers()) != 0); }
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
void AppMenuWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Backspace) {
        searchLineEdit->setFocus();
        // FIXME: Wait until searchLineEdit has focus before continuing
    }
    QCoreApplication::sendEvent(parent(), event);
}

void SearchLineEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Up) {
        emit editingFinished();
        QCoreApplication::sendEvent(parent(), event);
    } else {
        QCoreApplication::sendEvent(parent(), event);
    }
    QLineEdit::keyPressEvent(event);
}

class MyLineEditEventFilter : public QObject
{
public:
    explicit MyLineEditEventFilter(QLineEdit *parent) : QObject(parent) { }

    bool eventFilter(QObject *obj, QEvent *e)
    {
        // qDebug() << "probono: e->type()" << e->type();
        switch (e->type()) {
            case QEvent::WindowActivate: {
                // Whenever this window becomes active, then set the focus on the search box
                if (reinterpret_cast<QLineEdit *>(parent())->hasFocus() == false) {
                    reinterpret_cast<QLineEdit *>(parent())->setFocus();
                }
                break;
            }
            case QEvent::KeyPress: {
                QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
                // qDebug() << "probono: keyEvent->key()" << keyEvent->key();
                if (keyEvent->key() == Qt::Key_Escape) {
                    // When esc key is pressed while cursor is in QLineEdit, empty the QLineEdit
                    // https://stackoverflow.com/a/38066410
                    reinterpret_cast<QLineEdit *>(parent())->clear();
                    reinterpret_cast<QLineEdit *>(parent())->setText("");
                }
                if (keyEvent->key() == (Qt::Key_Tab | Qt::Key_Alt)) {
                    // When esc Tab is pressed while cursor is in QLineEdit, also empty the QLineEdit
                    // and prevent the focus from going elsewhere in the menu. This effectively prevents
                    // the menu from being operated by cursor keys. If we want that functionality back,
                    // we might remove the handling of Qt::Key_Tab but instead we would have to ensure
                    // that we put the focus back on the search box whenever this application is
                    // launched (again) and re-invoked by QSingleApplication
                    reinterpret_cast<QLineEdit *>(parent())->clear();
                    reinterpret_cast<QLineEdit *>(parent())->setText("");
                }
                break;
            }
            case QEvent::FocusOut: // QEvent::FocusOut:
            {
                // When the focus goes not of the QLineEdit, empty the QLineEdit and restore the
                // placeholder text reinterpret_cast<QLineEdit
                // *>(parent())->setPlaceholderText("Alt+Space"); reinterpret_cast<QLineEdit
                // *>(parent())->setPlaceholderText(tr("Search")); Note that we write Alt-Space here but
                // in fact this is not a feature of this application but is a feature of
                // lxqt-config-globalkeyshortcuts in our case, where we set up a shortcut that simply
                // launches this application (again). Since we are using
                // searchLineEdit->setStyleSheet("background: white"); // Do this in stylesheet.qss
                // instead reinterpret_cast<QLineEdit
                // *>(parent())->setAlignment(Qt::AlignmentFlag::AlignRight);
                reinterpret_cast<QLineEdit *>(parent())->clear();
                reinterpret_cast<QLineEdit *>(parent())->setText("");
                break;
            }
            case QEvent::FocusIn: {
                // When the focus goes into the QLineEdit, empty the QLineEdit
                // reinterpret_cast<QLineEdit *>(parent())->setPlaceholderText("");
                // reinterpret_cast<QLineEdit *>(parent())->setAlignment(Qt::AlignmentFlag::AlignLeft);
                break;
            }
        default:
            return QObject::eventFilter(obj, e);
        }
        return QObject::eventFilter(obj, e);
    }
};

// Select the first result of the completer if there is only one result left
// https://github.com/helloSystem/Menu/issues/14
class AutoSelectFirstFilter : public QObject
{
public:
    explicit AutoSelectFirstFilter(QLineEdit *parent) : QObject(parent) { }

    bool eventFilter(QObject *obj, QEvent *e) override
    {
        QCompleter *completer = reinterpret_cast<QLineEdit *>(parent())->completer();
        // Automatically select the first match of the completer if there is only one result left

        if (e->type() == QEvent::KeyRelease) {
            if (completer->completionCount() == 1) {
                // completer->setCurrentRow(0); // This is not changing the current row selection,
                // but the following does
                QListView *l = static_cast<QListView *>(completer->popup());
                QModelIndex idx = completer->completionModel()->index(0, 0, QModelIndex());
                l->setCurrentIndex(idx);
            }
        }
        return QObject::eventFilter(obj, e);
    }
};

void AppMenuWidget::addAppToMenu(QString candidate, QMenu *submenu)
{
    // qDebug() << "probono: Processing" << candidate;
    QString nameWithoutSuffix =
            QFileInfo(QDir(candidate).canonicalPath())
                    .completeBaseName(); // baseName() gets it wrong e.g., when there are dots in
                                         // version numbers; dereference symlink to candidate
    QFileInfo file(candidate);
    if (file.fileName().endsWith(".app")) {
        QString AppCand = QDir(candidate).canonicalPath() + "/"
                + nameWithoutSuffix; // Dereference symlink to candidate
        // qDebug() << "################### Checking" << AppCand;
        if (QFileInfo::exists(AppCand) == true) {
            // qDebug() << "# Found" << AppCand;
            QFileInfo fi(file.fileName());
            QString base = fi.completeBaseName(); // The name of the .app directory without suffix
                                                  // // baseName() gets it wrong e.g., when there
                                                  // are dots in version numbers
            QAction *action = submenu->addAction(base);
            connect(action, &QAction::triggered, this, [this, action] {
                actionLaunch(action);
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });
            searchResults
                    << action; // The items in searchResults get removed when search results change
            action->setToolTip(file.absoluteFilePath());
            action->setProperty("path", file.absoluteFilePath());
            QString IconCand =
                    QDir(candidate).canonicalPath() + "/Resources/" + nameWithoutSuffix + ".png";
            if (QFileInfo::exists(IconCand) == true) {
                // qDebug() << "#   Found icon" << IconCand;
                action->setIcon(QIcon(IconCand));
                action->setIconVisibleInMenu(true); // So that an icon is shown even though the
                                                    // theme sets Qt::AA_DontShowIconsInMenus
            }
        }
    } else if (file.fileName().endsWith(".AppDir")) {
        QString AppCand = QDir(candidate).canonicalPath() + "/" + "AppRun";
        // qDebug() << "################### Checking" << AppCand;
        if (QFileInfo::exists(AppCand) == true) {
            // qDebug() << "# Found" << AppCand;
            QFileInfo fi(file.fileName());
            QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are
                                                  // dots in version numbers
            QStringList executableAndArgs = { AppCand };
            QAction *action = submenu->addAction(base);
            connect(action, &QAction::triggered, this, [this, action] {
                actionLaunch(action);
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });
            searchResults
                    << action; // The items in searchResults get removed when search results change
            action->setToolTip(file.absoluteFilePath());
            action->setProperty("path", file.absoluteFilePath());
            QString IconCand = QDir(candidate).canonicalPath() + "/.DirIcon";
            if (QFileInfo::exists(IconCand) == true) {
                // qDebug() << "#   Found icon" << IconCand;
                action->setIcon(QIcon(IconCand));
                action->setIconVisibleInMenu(true); // So that an icon is shown even though the
                                                    // theme sets Qt::AA_DontShowIconsInMenus
            }
        }
    } else if (file.fileName().endsWith(".desktop")) {
        // .desktop file
        qDebug() << "# Found" << file.fileName();
        QFileInfo fi(file.fileName());
        QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots
                                              // in version numbers
        QStringList executableAndArgs = { fi.absoluteFilePath() };
        QSettings desktopFile(file.absoluteFilePath(), QSettings::IniFormat);
        QString noDisplayCand = desktopFile.value("Desktop Entry/NoDisplay").toString();
        if (noDisplayCand != "true") {
            QString name = desktopFile.value("Desktop Entry/Name").toString();
            QString IconCand = desktopFile.value("Desktop Entry/Icon").toString();
            if (name.isEmpty())
                name = base;
            QAction *action = submenu->addAction(name);
            connect(action, &QAction::triggered, this, [this, action] {
                actionLaunch(action);
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });
            searchResults
                    << action; // The items in searchResults get removed when search results change
            // Finding the icon file is way too involved with XDG, but we are not implementing all
            // edge cases If you were doubting that XDG standards are overly complex, here is the
            // proof...
            action->setIcon(QIcon::fromTheme(IconCand));
            QStringList iconSuffixes = { "", ".png", ".xpm", ".jpg", ".svg", ".icns" };
            if (IconCand.contains("/")) {
                if (QFileInfo::exists(IconCand)) {
                    action->setIcon(QIcon(IconCand));
                }
            } else if (QFileInfo("/usr/local/share/" + IconCand + "/icons/" + IconCand + ".png")
                               .exists()) {
                for (const QString iconSuffix : iconSuffixes) {
                    action->setIcon(QIcon("/usr/local/share/" + IconCand + "/icons/" + IconCand
                                          + iconSuffix));
                }
            } else {
                for (const QString iconSuffix : iconSuffixes) {
                    for (QString pixmapsPath :
                         QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
                        QString iconCandFile = pixmapsPath + "/pixmaps/" + IconCand + iconSuffix;
                        qDebug() << iconCandFile;
                        if (QFileInfo::exists(iconCandFile)) {
                            qDebug() << "Found!";
                            action->setIcon(QIcon(iconCandFile));
                        }
                    }
                }
            }

            action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme
                                                // sets Qt::AA_DontShowIconsInMenus
            action->setToolTip(file.absoluteFilePath());
            action->setProperty("path", file.absoluteFilePath());
            // action->setDisabled(true); // As a reminder that we consider those legacy and
            // encourage people to swtich
        }

    } else if (file.fileName().endsWith(".AppImage") || file.fileName().endsWith(".appimage")) {
        // .desktop file
        // qDebug() << "# Found" << file.fileName();
        QFileInfo fi(file.fileName());
        QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots
                                              // in version numbers
        QStringList executableAndArgs = { fi.absoluteFilePath() };
        QAction *action = submenu->addAction(base);
        connect(action, &QAction::triggered, this, [this, action] {
            actionLaunch(action);
            searchLineEdit->setText("");
            emit searchLineEdit->textChanged("");
            m_searchMenu->close();
        });
        searchResults
                << action; // The items in searchResults get removed when search results change
        action->setToolTip(file.absoluteFilePath());
        action->setProperty("path", file.absoluteFilePath());
        QString IconCand = Thumbnail(QDir(candidate).absolutePath(), QCryptographicHash::Md5,
                                     Thumbnail::ThumbnailSizeNormal, nullptr)
                                   .getIconPath();
        // qDebug() << "#   ############################### thumbnail for" <<
        // QDir(candidate).absolutePath();
        if (QFileInfo::exists(IconCand) == true) {
            // qDebug() << "#   Found thumbnail" << IconCand;
            action->setIcon(QIcon(IconCand));
            action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme
                                                // sets Qt::AA_DontShowIconsInMenus
        } else {
            // TODO: Request thumbnail;
            // https://github.com/KDE/kio-extras/blob/master/thumbnail/thumbnail.cpp qDebug() << "#
            // Did not find thumbnail" << IconCand << "TODO: Request it from thumbnailer";
        }
    } else if (file.isExecutable() && !file.isDir()) {
        // qDebug() << "# Found" << file.fileName();
        QFileInfo fi(file.fileName());
        QString base = fi.completeBaseName(); // baseName() gets it wrong e.g., when there are dots
                                              // in version numbers
        QStringList executableAndArgs = { fi.absoluteFilePath() };
        QAction *action = submenu->addAction(base);
        connect(action, &QAction::triggered, this, [this, action] {
            actionLaunch(action);
            searchLineEdit->setText("");
            emit searchLineEdit->textChanged("");
            m_searchMenu->close();
        });
        searchResults
                << action; // The items in searchResults get removed when search results change
        action->setToolTip(file.absoluteFilePath());
        action->setProperty("path", file.absoluteFilePath());
        action->setToolTip(file.absoluteFilePath());
        action->setProperty("path", file.absoluteFilePath());
        action->setIcon(QIcon::fromTheme("application-x-executable"));
        action->setIconVisibleInMenu(true); // So that an icon is shown even though the theme sets
                                            // Qt::AA_DontShowIconsInMenus
    }
}

void AppMenuWidget::findAppsInside(QStringList locationsContainingApps)
// probono: Check locationsContainingApps for applications and add them to the m_systemMenu.
// TODO: Nested submenus rather than flat ones with '▸'
// This code is similar to the code in the 'launch' command
{

    QStringList nameFilter({ "*.app", "*.AppDir", "*.desktop", "*.AppImage", "*.appimage" });
    foreach (QString directory, locationsContainingApps) {
        // Shall we process this directory? Only if it contains at least one application, to
        // optimize for speed by not descending into directory trees that do not contain any
        // applications at all. Can make a big difference.

        QDir dir(directory);
        int numberOfAppsInDirectory = dir.entryList(nameFilter).length();
        QMenu *submenu;

        if (directory.toLower().endsWith(".app") == false
            && directory.toLower().endsWith(".AppDir") == false && numberOfAppsInDirectory > 0) {
            // qDebug() << "# Descending into" << directory;
            QStringList locationsToBeChecked = { directory };
            // submenu = m_systemMenu->addMenu(base); // TODO: Use this once we have nested submenus
            // rather than flat ones with '→'
            submenu = m_systemMenu->addMenu(directory);
            submenu->setProperty("path", directory);

            // https://github.com/helloSystem/Menu/issues/15
            // probono: Watch this directory for changes and if we detect any, rebuild the menu
            if (!watchedLocations.contains(directory) && QFileInfo(directory).isDir()) {
                qDebug() << "Start watching" << directory;
                watchedLocations.append(directory);
                // probono: Without the next two lines, we get crashes
                // when we open a watched directory. Qt bug?
                watcher->~QFileSystemWatcher(); // probono: This is a crash workaround
                watcher = new QFileSystemWatcher(); // probono: This is a crash workaround
                QStringList failedToWatch = watcher->addPaths(watchedLocations);
                if (failedToWatch.length() > 0) {
                    qDebug() << "failedToWatch! Now a crash is imminent?";
                    qDebug() << "failedToWatch:" << failedToWatch;
                }
            }

            submenu->setToolTip(directory);
            submenu->setTitle(directory.remove(0, 1).replace("/", " ▸ "));
            submenu->setToolTipsVisible(true); // Seems to be needed here, too, so that the submenu
                                               // items show their correct tooltips?
            // Make it possible to open the directory that contains the app by clicking on the
            // submenu itself
            submenu->installEventFilter(this);
        } else {
            continue;
        }

        // Use QDir::entryList() insted of QDirIterator because it supports sorting
        QStringList candidates = dir.entryList();
        QString candidate;
        foreach (candidate, candidates) {
            candidate = dir.path() + "/" + candidate;
            // Do not show Autostart directories (or should we?)
            if (candidate.endsWith("/Autostart") == true) {
                continue;
            }
            QFileInfo file(candidate);
            if (locationsContainingApps.contains(candidate) == false && file.isDir()
                && candidate.endsWith("/..") == false && candidate.endsWith("/.") == false
                && candidate.endsWith(".app") == false && candidate.endsWith(".AppDir") == false) {
                // qDebug() << "# Found" << file.fileName() << ", a directory that is not an .app
                // bundle nor an .AppDir";
                QStringList locationsToBeChecked({ candidate });
                findAppsInside(locationsToBeChecked);
            } else {
                addAppToMenu(candidate, submenu);
            }
        }
    }
}
void iterate(const QModelIndex &index, const QAbstractItemModel *model,
             const std::function<int(const QModelIndex &, int depth)> &fun, int depth = 0)
{
    if (index.isValid())
        if (fun(index, depth) > 0) {
            return;
        }

    if ((index.flags() & Qt::ItemNeverHasChildren) || !model->hasChildren(index))
        return;
    auto rows = model->rowCount(index);
    auto cols = model->columnCount(index);
    for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
            iterate(model->index(i, j, index), model, fun, depth + 1);
}

AppMenuWidget::AppMenuWidget(QWidget *parent)
    : QWidget(parent), watcher(new QFileSystemWatcher(this)), m_typingTimer(new QTimer(this))
{
    // probono: Reload menu when something changed in a watched directory
    // https://github.com/helloSystem/Menu/issues/15
    connect(watcher, SIGNAL(directoryChanged(QString)), SLOT(rebuildMenu()));
    m_menuQCalc = new MenuQCalc();

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setAlignment(Qt::AlignCenter); // Center QHBoxLayout vertically
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);

    // Add search box to menu
    searchLineEdit = new SearchLineEdit(this);

    // Make sure the search box gets cleared when this application loses focus
    searchLineEdit->setObjectName(
            "actionSearch"); // probono: This name can be used in qss to style it specifically
    searchLineEdit->setFixedHeight(
            22); // FIXME: Dynamically get the height of a QMenuItem and use that
    searchLineEdit->setWindowFlag(Qt::WindowDoesNotAcceptFocus, false);
    searchLineEdit->setFocus();
    m_searchMenu = new QMenu();
    m_searchMenu->setIcon(QIcon::fromTheme("search-symbolic"));
    std::function<int(QModelIndex idx, int depth)> traverse = [this](QModelIndex idx, int depth) {
        QAction *action = idx.data().value<QAction *>();
        action->setShortcutContext(Qt::ApplicationShortcut);
        if (action->isVisible() && idx.parent().isValid()) {
            m_wasVisible.push_back(
                    cmpAction({ idx.parent().data().value<QAction *>()->text().toStdString(),
                                idx.data().value<QAction *>()->text().toStdString(), idx.row() }));
        }
        if (action->menu()) {
            emit action->menu()->aboutToShow();
        }

        return 0;
    };
    std::function<int(QModelIndex idx, int depth)> traverse1 = [this](QModelIndex idx, int depth) {
        QAction *action = idx.data().value<QAction *>();
        if (action->menu()) {
            emit action->menu()->aboutToShow();
        }
        return 0;
    };
    connect(m_searchMenu, &QMenu::aboutToShow, [this, traverse1]() {
        iterate(QModelIndex(), m_appMenuModel, traverse1);
        searchLineEdit->setFocus();
    });
    connect(qApp, &QApplication::focusWindowChanged, this, [this](QWindow *a) {
        // https://github.co    m/helloSystem/Menu/issues/95
    });
    connect(qApp, &QApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {

            m_searchMenuOpened = searchLineEdit->isActiveWindow();
        }
        if (state == Qt::ApplicationInactive && m_searchMenuOpened) {
            searchLineEdit->clear();
            emit searchLineEdit->textChanged("");
            m_searchMenuOpened = false;
        }
    });

    setFocusPolicy(Qt::NoFocus);
    // Prepare System menu
    m_systemMenu = new SystemMenu(this); // Using our SystemMenu subclass instead of a QMenu to be
                                         // able to toggle "About..." when modifier key is pressed
    m_systemMenu->setTitle(tr("System"));
    QWidgetAction *widgetAction = new QWidgetAction(this);
    widgetAction->setDefaultWidget(searchLineEdit);
    m_searchMenu->addAction(widgetAction);

    connect(searchLineEdit, &QLineEdit::editingFinished, this, &AppMenuWidget::searchEditingDone);

    // connect(searchLineEdit,&QLineEdit::textChanged,this,&AppMenuWidget::searchMenu);
    // Do not do this immediately, but rather delayed
    // https://wiki.qt.io/Delay_action_to_wait_for_user_interaction
    m_typingTimer->setSingleShot(true); // Ensure the timer will fire only once after it was started
    connect(m_typingTimer, &QTimer::timeout, this, &AppMenuWidget::searchMenu);
    connect(searchLineEdit, &QLineEdit::textChanged, this, &AppMenuWidget::refreshTimer);

    m_systemMenu->setToolTipsVisible(true); // Works; shows the full path

    // If we were using a QMenu, we would do:
    // QAction *aboutAction = m_systemMenu->addAction(tr("About This Computer"));
    // connect(aboutAction, SIGNAL(triggered()), this, SLOT(actionAbout()));
    // Since we are using our SystemMenu subclass instead which already contains the first menu
    // item, we do:
    connect(m_systemMenu->actions().constFirst(), SIGNAL(triggered()), this, SLOT(actionAbout()));

    m_systemMenu->addSeparator();

    // Search menu item, so that we have a place to show the shortcut in the menu (discoverability!)
    QAction *searchAction = m_systemMenu->addAction(tr("Search"));
    searchAction->setObjectName("Search"); // Needed for KGlobalAccel global shortcut; becomes
                                           // visible in kglobalshortcutsrc
    KGlobalAccel::self()->setShortcut(
            searchAction, { QKeySequence("Ctrl+Space") },
            KGlobalAccel::NoAutoloading); // Set global shortcut; this also becomes editable in
                                          // kglobalshortcutsrc
    connect(searchAction, &QAction::triggered, this, [searchAction, parent, this]() {
        qobject_cast<MainWidget *>(parent)->triggerFocusMenu();
        emit menuAboutToBeImported(); // Stop showing application name upon Command+Space; this gets
                                      // stopShowingApplicationName called
    });

    searchAction->setShortcut(QKeySequence(
            KGlobalAccel::self()
                    ->globalShortcut(qApp->applicationName(), searchAction->objectName())
                    .value(0))); // Show the shortcut on the menu item

    m_systemMenu->addSeparator();

    // Add submenus with applications to the System menu
    QStringList locationsContainingApps = {};
    locationsContainingApps.append(QDir::homePath());
    locationsContainingApps.append(
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    locationsContainingApps.append(QDir::homePath() + "/Applications");
    locationsContainingApps.append(QDir::homePath() + "/bin");
    locationsContainingApps.append(QDir::homePath() + "/.bin");
    locationsContainingApps.append("/Applications");
    locationsContainingApps.removeDuplicates(); // Make unique
    findAppsInside(locationsContainingApps);
    m_systemMenu->addSeparator();
    QAction *forceQuitAction = m_systemMenu->addAction(tr("Force Quit Application"));
    connect(forceQuitAction, SIGNAL(triggered()), this, SLOT(actionForceQuit()));
    forceQuitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Escape));
    m_systemMenu->addSeparator();

    /*
    // Sleep
    QAction *sleepAction = m_systemMenu->addAction(tr("Sleep"));
    sleepAction->setObjectName("Sleep"); // Needed for KGlobalAccel global shortcut; becomes visible
    in kglobalshortcutsrc
    // KGlobalAccel::self()->setShortcut(sleepAction, {QKeySequence("...")},
    KGlobalAccel::NoAutoloading); // Set global shortcut; this also becomes editable in
    kglobalshortcutsrc connect(sleepAction, &QAction::triggered, this, [sleepAction, this]() {
        qDebug() << __func__;
        QProcess *p = new QProcess();
        p->setProgram("zzz");
        p->setArguments({});
        p->startDetached();
    });
    m_systemMenu->addSeparator();
    */

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

    connect(m_appMenuModel, &AppMenuModel::menuAboutToBeImported, this,
            &AppMenuWidget::menuAboutToBeImported);
    connect(m_appMenuModel, &AppMenuModel::menuImported, this,
            [this, traverse](QString serviceName) {
                m_wasVisible.clear();
                if (m_appMenuModel->menuAvailable()) {

                    QTimer::singleShot(100, this, [this, traverse] {
                        iterate(QModelIndex(), m_appMenuModel, traverse);
                    });
                }
                m_appMenuModel->m_pending_service[serviceName] = false;
            });
    connect(m_appMenuModel, &AppMenuModel::menuAvailableChanged, this, &AppMenuWidget::updateMenu);

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this,
            &AppMenuWidget::delayUpdateActiveWindow);
    connect(KWindowSystem::self(),
            static_cast<void (KWindowSystem::*)(WId, NET::Properties, NET::Properties2)>(
                    &KWindowSystem::windowChanged),
            this, &AppMenuWidget::onWindowChanged);

    // Load action search
    actionCompleter = nullptr;
    MenuImporter *menuImporter = new MenuImporter(this);
    menuImporter->connectToBus();
}

void AppMenuWidget::searchEditingDone()
{
    if (m_searchMenu && m_searchMenu->actions().count() > 1) {
        searchLineEdit->clearFocus();
        for (QAction *findActivateeCanidcate : m_searchMenu->actions())
            if (!findActivateeCanidcate->isSeparator()) {
                m_searchMenu->setActiveAction(findActivateeCanidcate);
                break;
            }
    }
}

void AppMenuWidget::refreshTimer()
{
    m_typingTimer->start(300); // https://wiki.qt.io/Delay_action_to_wait_for_user_interaction
}

void AppMenuWidget::focusMenu()
{
    QMouseEvent event(QEvent::MouseButtonPress, QPoint(20, 0), m_menuBar->mapToGlobal(QPoint(0, 0)),
                      Qt::LeftButton, 0, 0);
    QApplication::sendEvent(m_menuBar, &event);
    searchLineEdit->setFocus();
}
AppMenuWidget::~AppMenuWidget() { }

void AppMenuWidget::integrateSystemMenu(QMenuBar *menuBar)
{
    if (!menuBar || !m_systemMenu)
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

void AppMenuWidget::updateActionSearch()
{

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
    // static_cast<QListView *>(actionCompleter->popup())->setContentsMargins(10,10,0,10); // FIXME:
    Does not seem to work, why?

    // Empty search field on selection of an item, https://stackoverflow.com/a/11905995

    //QObject::connect(actionCompleter, SIGNAL(activated(const QString&)),
      //               searchLineEdit, SLOT(clear()),
        //             Qt::QueuedConnection);
*/
    // Make more than 7 items visible at once
    // actionCompleter->setMaxVisibleItems(35);

    // compute needed width
    // const QAbstractItemView * popup = actionCompleter->popup();
    // actionCompleter->popup()->setMinimumWidth(350);
    // actionCompleter->popup()->setMinimumWidth(600);

    // actionCompleter->popup()->setContentsMargins(100,100,100,100);

    // Make the completer match search terms in the middle rather than just those at the beginning
    // of the menu
    // actionCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    // actionCompleter->setFilterMode(Qt::MatchContains);

    // Set first result active; https://stackoverflow.com/q/17782277. FIXME: This does not work yet.
    // Why?
    // QItemSelectionModel* sm = new QItemSelectionModel(actionCompleter->completionModel());
    // actionCompleter->popup()->setSelectionModel(sm);
    // sm->select(actionCompleter->completionModel()->index(0,0), QItemSelectionModel::Select);

    // auto* flt = new AutoSelectFirstFilter(searchLineEdit);
    // actionCompleter->popup()->installEventFilter(flt);

    // actionCompleter->popup()->setAlternatingRowColors(false);
    //  actionCompleter->popup()->setStyleSheet("QListView::item { color: green; }"); // FIXME: Does
    //  not work. Why?
    // searchLineEdit->setCompleter(actionCompleter);
    //  Sort results of the Action Search
    // actionCompleter->completionModel()->sort(0,Qt::SortOrder::AscendingOrder);
}

void AppMenuWidget::searchMenu()
{
    QString searchString = searchLineEdit->text();
    for (QAction *sr : qAsConst(searchResults)) {
        if (m_searchMenu->actions().contains(sr)) {
            m_searchMenu->removeAction(sr);
            CloneAction *ca = qobject_cast<CloneAction *>(sr);
            if (ca) {
                ca->resetOrigShortcutContext();
                ca->disconnectOnClear();
            }
        }
    }

    QMimeDatabase mimeDatabase;
    if (searchString.startsWith("= ")) {
        QString result = m_menuQCalc->getResult(searchString.remove(0, 1).trimmed(), true);
        QIcon icon = QIcon::fromTheme("accessories-calculator");
        QAction *res = new QAction(result);
        res->setIcon(icon);
        res->setIconVisibleInMenu(true);
        m_searchMenu->addAction(res);
        searchResults << res;

        qDebug() << result;
        return;
    }
    // Only initialize fscompleter if searhcstring hints a path;
    if (searchString.startsWith("/") || searchString == "~") {

        if (searchString == "~") {
            searchLineEdit->setText(QDir::homePath() + "/");
            searchLineEdit->textChanged(QDir::homePath() + "/");
            return;
        }

        QString dirPath = searchString.mid(0, searchString.lastIndexOf("/") + 1);
        qDebug() << dirPath << __LINE__;
        if (QFileInfo(dirPath).isDir()) {

            QFileInfo fInfo = QFileInfo(dirPath);
            if (fInfo.exists() && fInfo.isDir()) {
                QDir dir(dirPath);
                m_searchMenu->addSeparator();
                foreach (QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllDirs)) {
                    if (info.isDir()) {
                        if (info.fileName().startsWith(
                                    searchString.mid(searchString.lastIndexOf("/"), -1)
                                            .remove(0, 1))) {
                            QAction *res = new QAction(info.fileName());
                            // Folder icon
                            QIcon icon = QIcon::fromTheme("folder");
                            res->setIcon(icon);
                            res->setIconVisibleInMenu(true);
                            res->setProperty("path", info.filePath());
                            connect(res, &QAction::triggered, this, [this, res] {
                                openPath(res);
                                searchLineEdit->setText("");
                                emit searchLineEdit->textChanged("");
                                m_searchMenu->close();
                            });
                            m_searchMenu->addAction(res);
                            searchResults << res;
                        }
                    }
                }
            }
        }

        return;
    }
    // If the search first word is found on the $PATH, use it like a launcher does
    // TODO: Only do this if we have NOT found applications with the same name

    // Check whether it is on the $PATH and is executable
    if (searchString != "") {
        QString mathRes = m_menuQCalc->getResult(searchString, false);
        if (mathRes != "") {
            QIcon icon = QIcon::fromTheme("accessories-calculator");
            QAction *res = new QAction(mathRes);
            res->setIcon(icon);
            res->setIconVisibleInMenu(true);
            m_searchMenu->addAction(res);
            searchResults << res;
        }
        QString command = searchString.split(" ").first();
        QString pathEnv = getenv("PATH");
        QStringList directories = pathEnv.split(":");
        bool found = false;
        for (const QString &directory : directories) {
            QFile file(directory + "/" + command);
            if (file.exists() && (file.permissions() & QFileDevice::ExeUser)) {
                found = true;
                break;
            }
        }

        if (found) {
            qDebug() << command << "found on the $PATH";

            m_searchMenu->addSeparator();

            QAction *res = new QAction();
            res->setText(searchString);
            res->setToolTip(searchString);
            QIcon icon = QIcon::fromTheme("terminal");
            res->setIcon(icon);
            res->setIconVisibleInMenu(true);
            res->setProperty("path", searchString);
            connect(res, &QAction::triggered, this, [this, res] {
                QProcess p;
                p.setProgram("launch");
                QString path = res->property("path").toString();
                QStringList arguments = QProcess::splitCommand(path);
                arguments.prepend("-e");
                arguments.prepend("QTerminal"); // FIXME: Would be nice to keep it open;
                                                // https://github.com/lxqt/qterminal/issues/1030
                qDebug() << "Executing:" << p.program(), p.arguments();
                p.setArguments(arguments);
                p.startDetached();
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });

            m_searchMenu->addAction(res);
            searchResults
                    << res; // The items in searchResults get removed when search results change
        }
    }

    // If the search starts with "/", then see whether we have a path there
    if (searchString.startsWith("/") || searchString.startsWith("~")) {

        m_searchMenu->addSeparator();

        searchString.replace("~", QDir::homePath());

        if (QFileInfo::exists(searchString) == true) {
            QMimeType mimeType;
            mimeType = mimeDatabase.mimeTypeForFile(QFileInfo(searchString));
            QAction *res = new QAction();
            res->setText(searchString);
            res->setToolTip(searchString);
            QIcon icon = QIcon::fromTheme(mimeType.iconName());
            res->setIcon(icon);
            res->setIconVisibleInMenu(true);
            res->setProperty("path", searchString);
            connect(res, &QAction::triggered, this, [this, res] {
                openPath(res);
                qDebug() << res << __LINE__;
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });

            m_searchMenu->addAction(res);
            searchResults
                    << res; // The items in searchResults get removed when search results change
        }

        return; // Don't show any other results in this case
    }

    std::function<int(QModelIndex idx, int depth)> setResultVisbileMbar = [this, searchString](
                                                                                  QModelIndex idx,
                                                                                  int depth) {
        QAction *action = idx.data().value<QAction *>();
        if (searchString.isEmpty()) {

            if (idx.parent().isValid()) {
                std::vector<cmpAction>::iterator it = std::find(
                        m_wasVisible.begin(), m_wasVisible.end(),
                        cmpAction({ idx.parent().data().value<QAction *>()->text().toStdString(),
                                    idx.data().value<QAction *>()->text().toStdString(),
                                    idx.row() }));
                bool visible = it != m_wasVisible.end();
                action->setVisible(visible);
                QModelIndex p = idx.parent();
                while (p.isValid()) {
                    p.data().value<QAction *>()->setVisible(true);
                    p = p.parent();
                }
            }

            return 0;
        }

        bool visible = false;
        if (idx.parent().isValid()) {
            cmpAction cmp1 = { idx.parent().data().value<QAction *>()->text().toStdString(),
                               idx.data().value<QAction *>()->text().toStdString(), idx.row() };
            std::vector<cmpAction>::iterator it =
                    std::find(m_wasVisible.begin(), m_wasVisible.end(), cmp1);
            visible = it != m_wasVisible.end();
        }
        if (!searchString.isEmpty() && action->text().contains(searchString, Qt::CaseInsensitive)) {
            action->setVisible(true && visible);
            QModelIndex p = idx.parent();
            QStringList names;
            while (p.isValid()) {
                p.data().value<QAction *>()->setVisible(true);
                names << p.data().value<QAction *>()->text().replace("&", "");
                p = p.parent();
            }
            std::reverse(names.begin(), names.end());

            QAction *orig = idx.data().value<QAction *>();
            if (!orig->menu()) {
                CloneAction *cpy = new CloneAction(orig);

                cpy->setText(names.join(" ▸ ") + " ▸ " + orig->text());
                cpy->setShortcut(orig->shortcut());
                orig->setShortcutContext(Qt::WindowShortcut);
                cpy->setShortcutContext(Qt::ApplicationShortcut);
                cpy->updateMe();
                searchResults << cpy;
                connect(cpy, &QAction::triggered, this, [this] {
                    searchLineEdit->setText("");
                    searchLineEdit->textChanged("");
                    m_searchMenu->close();
                });
                cpy->setDisconnectOnClear(connect(orig, &QAction::triggered, this, [this] {
                    searchLineEdit->setText("");
                    searchLineEdit->textChanged("");
                    m_searchMenu->close();
                }));
                m_searchMenu->addAction(cpy);
            }
        }

        else if (!searchString.isEmpty()) {
            if (!visible && action->isVisible() && idx.parent().isValid()) {
                m_wasVisible.push_back(
                        { idx.parent().data().value<QAction *>()->text().toStdString(),
                          idx.data().value<QAction *>()->text().toStdString(), idx.row() });
            }
            action->setVisible(false);
        }

        return 0;
    };

    searchResults << m_searchMenu->addSeparator(); // The items in searchResults get removed when
                                                   // search results change

    iterate(QModelIndex(), m_appMenuModel, setResultVisbileMbar);
    m_isSearching = false;

    searchResults << m_searchMenu->addSeparator(); // The items in searchResults get removed when
                                                   // search results change

    QList<QMenu *> menus;
    menus << m_systemMenu;
    QStringList names;

    for (QMenu *menu : qAsConst(menus)) {
        m_appMenuModel->filterMenu(menu, searchString, searchString == "", names);
    }
    const QStringList keys = m_appMenuModel->filteredActions().keys();
    for (const QString &v : keys) {
        QAction *orig = m_appMenuModel->filteredActions()[v];
        CloneAction *cpy = new CloneAction(orig);
        cpy->setText(v);
        cpy->setShortcut(orig->shortcut());
        cpy->setToolTip(orig->toolTip());
        cpy->updateMe();
        cpy->setShortcutContext(Qt::ApplicationShortcut);
        orig->setShortcutContext(Qt::WindowShortcut);
        searchResults << cpy;
        connect(cpy, &QAction::triggered, this, [this] {
            searchLineEdit->setText("");
            emit searchLineEdit->textChanged("");
            m_searchMenu->close();
        });
        cpy->setDisconnectOnClear(connect(orig, &QAction::triggered, this, [this] {
            searchLineEdit->setText("");
            emit searchLineEdit->textChanged("");
            m_searchMenu->close();
        }));
        m_searchMenu->addAction(cpy);
    }

    // probono: Use Baloo API and add baloo search results to the Search menu; see below for a
    // rudimentary non-API version
    if (searchString != "" && !searchString.startsWith("/")) {
        searchResults << m_searchMenu->addSeparator(); // The items in searchResults get removed
                                                       // when search results change
        Baloo::Query query;
        query.setSearchString(searchString);
        query.setLimit(21);
        Baloo::ResultIterator iter = query.exec();
        int i = 0;
        bool showMore = false;
        while (iter.next()) {
            i++;
            if (i == query.limit()) {
                showMore = true;
                break;
            }
            QMimeType mimeType;
            mimeType = mimeDatabase.mimeTypeForFile(QFileInfo(iter.filePath()));
            QAction *res = new QAction();
            res->setText(iter.filePath().split("/").last());
            res->setToolTip(iter.filePath());

            // If it is an application, add it to the menu using the application's icon
            if (iter.filePath().toLower().endsWith(".desktop")
                || iter.filePath().toLower().endsWith(".app")
                || iter.filePath().toLower().endsWith(".appdir")
                || iter.filePath().toLower().endsWith(".appimage")) {
                addAppToMenu(iter.filePath(), m_searchMenu);
                continue;
            }

            // If it is a file that has the executable bit set, also add it to the menu
            if (QFileInfo(iter.filePath()).isExecutable()
                && not QFileInfo(iter.filePath()).isDir()) {
                addAppToMenu(iter.filePath(), m_searchMenu);
                continue;
            }

            // If there is a thumbnail, show it
            QString IconCand =
                    Thumbnail(QDir(iter.filePath()).absolutePath(), QCryptographicHash::Md5,
                              Thumbnail::ThumbnailSizeNormal, nullptr)
                            .getIconPath();
            // qDebug() << "#   ############################### thumbnail for" <<
            // QDir(iter.filePath()).absolutePath();
            if (QFileInfo::exists(IconCand) == true) {
                // qDebug() << "#   Found thumbnail" << IconCand;
                res->setIcon(QIcon(IconCand));
            } else {
                // TODO: Request thumbnail;
                // https://github.com/KDE/kio-extras/blob/master/thumbnail/thumbnail.cpp qDebug() <<
                // "#   Did not find thumbnail" << IconCand << "TODO: Request it from thumbnailer";
                QIcon icon = QIcon::fromTheme(mimeType.iconName());
                res->setIcon(icon);
            }

            res->setIconVisibleInMenu(true);
            res->setProperty("path", iter.filePath());
            connect(res, &QAction::triggered, this, [this, res] {
                openPath(res);
                searchLineEdit->setText("");
                emit searchLineEdit->textChanged("");
                m_searchMenu->close();
            });

            m_searchMenu->addAction(res);
            searchResults
                    << res; // The items in searchResults get removed when search results change
        }

        if (showMore) {
            QAction *a = new QAction();
            searchResults << a; // The items in searchResults get removed when search results change
            a->setText("...");
            a->setDisabled(true);
            m_searchMenu->addAction(a);
        }
    }

    /*
        // probono: query baloosearch and add baloo search results to the Search menu; see above for
       an API version

        QProcess p;
        QString program = "baloosearch";
        QStringList arguments;
        arguments << "-l" << "20" << searchString;
        p.start(program, arguments);
        p.waitForFinished();
        QStringList balooSearchResults;
        QString result(p.readAllStandardOutput());
        if(result != ""){
            // m_searchMenu->addSeparator(); // FIXME: Would be nicer but breaks autoselection when
       only one search result is left balooSearchResults = result.split('\n'); for(QString
       searchResult: balooSearchResults){ if(! searchResult.startsWith("/")) { continue;
                }
                qDebug() << "probono: searchResult:" << searchResult;
                QAction *orig = new QAction();
                CloneAction *a = new CloneAction(orig); // Using a CloneAction so that these search
       results get removed like menu search results; FIXME: Do more efficiently
                a->setToolTip(searchResult);
                a->setText(searchResult.split("/").last());
                searchResults << a; // The items in searchResults get removed when search results
       change connect(a,&QAction::triggered,this,[this, a]{ openBalooSearchResult(a);
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
                a->setIconVisibleInMenu(true); // So that an icon is shown even though the theme
       sets Qt::AA_DontShowIconsInMenus a->setProperty("path", searchResult);
                m_searchMenu->addAction(a);
            }
            if(balooSearchResults.length()>20) {
                CloneAction *a = new CloneAction(new QAction()); // Using a CloneAction so that
       these search results get removed like menu search results; FIXME: Do more efficiently
                searchResults << a; // The items in searchResults get removed when search results
       change a->setText("..."); a->setDisabled(true); m_searchMenu->addAction(a);
            }
        }
    */

    // If there is only one search result, select it
    int number_of_enabled_actions = 0;
    const QList<QAction *> actions = m_searchMenu->actions();
    for (QAction *a : actions) {
        if (a->isEnabled())
            number_of_enabled_actions++;
    }
    qDebug() << "probono: number_of_enabled_actions" << number_of_enabled_actions;
    // QUESITON: Unclear whether it is 2 or 3, depending on whether one menu action or one Baloo
    // search result is there...
    if (number_of_enabled_actions == 2
        || (number_of_enabled_actions == 3 && m_appMenuModel->filteredActions().count() == 1)) {
        QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
        // searchEditingDone();
        QCoreApplication::sendEvent(searchLineEdit, event);
    } else {
        auto evt = new QMouseEvent(QEvent::MouseMove,
                                   m_menuBar->actionGeometry(m_menuBar->actions().at(0)).center(),
                                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::postEvent(m_menuBar, evt);
    }
    m_appMenuModel->clearFilteredActions();
}

void AppMenuWidget::rebuildMenu()
{
    qDebug() << "AppMenuWidget::rebuildMenu() called";
    qobject_cast<MainWidget *>(parent())->rebuildSystemMenu();
}

void AppMenuWidget::updateMenu()
{
    if (!m_appMenuModel->menuAvailable()) {
        // This clears the menu in case it has no entries
        // probono: If no menus are available, we insert fallback ones

        int cnt = m_menuBar->actions().count();
        QList<QAction *> remove;
        for (int i = 2; i < cnt; i++) {
            remove.append(m_menuBar->actions().at(i));
        }
        for (QAction *r : remove) {
            m_menuBar->removeAction(r);
        }

        emit menuAboutToBeImported(); // misnomer there is no menu
        m_appMenuModel->invalidateMenu();

        qDebug() << "No menus are available. Insert fallback ones";

        // Add fallback menus
        // for applications that did not send menus
        WId winId = KWindowSystem::activeWindow();
        KWindowInfo info(winId, NET::WMPid | NET::WMWindowType);
        QMenu *fallbackFileMenu = this->m_menuBar->addMenu(tr("File"));
        QAction *closeAction = new QAction(tr("Close"));
        closeAction->setShortcut(QKeySequence("Ctrl+W"));
        closeAction->setObjectName("close");
        // Register as a global shortcut that cannot be overridden by the application
        KGlobalAccel::self()->setShortcut(closeAction, QList<QKeySequence>(),
                                          KGlobalAccel::NoAutoloading);
        connect(closeAction, &QAction::triggered, [=]() {
            // Simulate sending Alt+F4 to the application
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_F4, Qt::AltModifier);
            QCoreApplication::sendEvent(QApplication::focusWidget(), event);
        });
        fallbackFileMenu->addAction(closeAction);
        fallbackFileMenu->addSeparator();
        QAction *quitAction = new QAction(tr("Quit"));
        quitAction->setShortcut(QKeySequence("Ctrl+Q"));
        quitAction->setObjectName("quit");
        // Register as a global shortcut that cannot be overridden by the application
        KGlobalAccel::self()->setShortcut(quitAction, QList<QKeySequence>(),
                                          KGlobalAccel::NoAutoloading);
        connect(quitAction, &QAction::triggered, [=]() {
            QWidget *w = QApplication::focusWidget();
            kill(info.pid(), SIGINT);
            QKeyEvent *event = new QKeyEvent(QEvent::KeyPress, Qt::Key_F4, Qt::AltModifier);
            QCoreApplication::sendEvent(w, event);
        });

        fallbackFileMenu->addAction(quitAction);

        QMenu *fallbackEditMenu = m_menuBar->addMenu(tr("Edit"));
        QAction *undoAction = new QAction(tr("Undo"));
        undoAction->setShortcut(QKeySequence("Ctrl+Z"));
        connect(undoAction, &QAction::triggered, [=]() {
            QProcess::startDetached("xdotool", { "getactivewindow", "key", "ctrl+z" });
            /* FIXME:
                  * This would be the better way, but getting: undefined symbol:
               xcb_key_symbols_get_keycode xcb_connection_t *c = QX11Info::connection(); const
               xcb_window_t w = QX11Info::appRootWindow(); const Display *dpy = XOpenDisplay(NULL);
                 const xcb_keysym_t sym_z = XStringToKeysym("z");
                 const xcb_keysym_t sym_x = XStringToKeysym("x");
                 const xcb_keysym_t sym_c = XStringToKeysym("c");
                 const xcb_keysym_t sym_v = XStringToKeysym("v");
                 constexpr xcb_keysym_t ctrl = 0xffe3;
                 xcb_key_symbols_t *syms = xcb_key_symbols_alloc(c);
                 auto getCode = [syms](int code) {
                     xcb_keycode_t *keyCodes = xcb_key_symbols_get_keycode(syms, code);
                     const xcb_keycode_t ret = keyCodes[0];
                     free(keyCodes);
                     return ret;
                 };
                 xcb_test_fake_input(c, XCB_KEY_PRESS, getCode(ctrl), XCB_CURRENT_TIME, XCB_NONE, 0,
               0, 0); xcb_test_fake_input(c, XCB_KEY_PRESS, getCode(sym_c), XCB_CURRENT_TIME,
               XCB_NONE, 0, 0, 0); xcb_test_fake_input(c, XCB_KEY_RELEASE, getCode(sym_c),
               XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0); xcb_test_fake_input(c, XCB_KEY_RELEASE,
               getCode(ctrl), XCB_CURRENT_TIME, XCB_NONE, 0, 0, 0); xcb_flush(c);
                 */
        });
        fallbackEditMenu->addAction(undoAction);
        fallbackEditMenu->addSeparator();
        QAction *cutAction = new QAction(tr("Cut"));
        cutAction->setShortcut(QKeySequence("Ctrl+X"));
        connect(cutAction, &QAction::triggered, [=]() {
            QProcess::startDetached("xdotool", { "getactivewindow", "key", "ctrl+x" });
        });
        QAction *copyAction = new QAction(tr("Copy"));
        copyAction->setShortcut(QKeySequence("Ctrl+C"));
        connect(copyAction, &QAction::triggered, [=]() {
            QProcess::startDetached("xdotool", { "getactivewindow", "key", "ctrl+c" });
        });
        QAction *pasteAction = new QAction(tr("Paste"));
        pasteAction->setShortcut(QKeySequence("Ctrl+V"));
        connect(pasteAction, &QAction::triggered, [=]() {
            QProcess::startDetached("xdotool", { "getactivewindow", "key", "ctrl+v" });
        });
        fallbackEditMenu->addAction(cutAction);
        fallbackEditMenu->addAction(copyAction);
        fallbackEditMenu->addAction(pasteAction);
    }
}

void AppMenuWidget::toggleMaximizeWindow()
{
    KWindowInfo info(KWindowSystem::activeWindow(), NET::WMState);
    bool isMax = info.hasState(NET::Max);
    bool isWindow = !info.hasState(NET::SkipTaskbar)
            || info.windowType(NET::UtilityMask) != NET::Utility
            || info.windowType(NET::DesktopMask) != NET::Desktop;

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

    KWindowInfo info(id, NET::WMWindowType | NET::WMState,
                     NET::WM2TransientFor | NET::WM2WindowClass);

    if (!info.valid())
        return false;

    if (NET::typeMatchesMask(info.windowType(NET::AllTypesMask), ignoreList))
        return false;

    if (info.state() & NET::SkipTaskbar)
        return false;

    // WM_TRANSIENT_FOR hint not set - normal window
    WId transFor = info.transientFor();
    if (transFor == 0 || transFor == id || transFor == (WId)QX11Info::appRootWindow())
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
        // TODO: Need to trigger updating the menu here? Sometimes it stays blank after an
        // application has been closed
    }

    if (m_currentWindowID > 0 && m_currentWindowID != m_windowID && m_windowID != 0) {

        // searchLineEdit->clear();
        // searchLineEdit->textChanged("");

        // bool isMax = info.hasState(NET::Max);
    }

    m_currentWindowID = m_windowID;
}

void AppMenuWidget::onWindowChanged(WId /*id*/, NET::Properties /*properties*/,
                                    NET::Properties2 /*properties2*/)
{
    if (m_windowID && m_windowID == KWindowSystem::activeWindow())
        onActiveWindowChanged();
}

void AppMenuWidget::minimizeWindow()
{
    KWindowSystem::minimizeWindow(KWindowSystem::activeWindow());
}

void AppMenuWidget::closeWindow()
{
    NETRootInfo(QX11Info::connection(), NET::CloseWindow)
            .closeWindowRequest(KWindowSystem::activeWindow());
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

    AboutDialog *dialog = new AboutDialog(this);

    // Center window on screen
    QRect rec = QGuiApplication::screenAt(this->pos())->geometry();
    QSize size = dialog->sizeHint();
    QPoint topLeft = QPoint((rec.width() / 2) - (size.width() / 2),
                            (rec.height() / 2) - (size.height() / 2));
    dialog->setGeometry(QRect(topLeft, size));

    dialog->show();
}

void AppMenuWidget::actionLaunch(QAction *action)
{
    qDebug() << "actionLaunch(QAction *action) called";
    // Setting a busy cursor in this way seems only to affect the own application's windows
    // rather than the full screen, which is why it is not suitable for this application
    // QApplication::setOverrideCursor(Qt::WaitCursor);
    QString pathToBeLaunched = action->property("path").toString();
    if (QApplication::keyboardModifiers()) {
        // Just show the file in the file manager when a modifier key is pressed
        QDBusInterface interface("org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
                                 "org.freedesktop.FileManager1");
        interface.call(QDBus::NoBlock, "ShowItems",
                       QStringList({ QUrl::fromLocalFile(pathToBeLaunched).toEncoded() }),
                       ""); // Need URL here!
    } else {
        QProcess::startDetached("launch", { pathToBeLaunched });
    }
}

// probono: When a modifier key is held down, then just show the item in Filer;
// otherwise launch it if it is an application bundle, or open it if it is not
void AppMenuWidget::openPath(QAction *action)
{
    QString pathToBeLaunched = action->property("path").toString();
    if (QApplication::keyboardModifiers()) {
        // Just show the file in the file manager when a modifier key is pressed
        QDBusInterface interface("org.freedesktop.FileManager1", "/org/freedesktop/FileManager1",
                                 "org.freedesktop.FileManager1");
        interface.call(QDBus::NoBlock, "ShowItems",
                       QStringList({ QUrl::fromLocalFile(pathToBeLaunched).toEncoded() }),
                       ""); // Need URL here!
    } else {
        QProcess p;
        p.setArguments({ pathToBeLaunched });
        if (pathToBeLaunched.endsWith(".app") || pathToBeLaunched.endsWith(".AppDir")
            || pathToBeLaunched.endsWith(".AppImage")) {
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
    // TODO: In a similar way, implement "Hide <window name>" and "Hide others". For this we need to
    // know the window ID of the frontmost application window
    qDebug() << "probono: KWindowSystem::activeWindow;"
             << "0x" + QString::number(KWindowSystem::activeWindow(), 16);
    // NOTE: This always prints the window ID of the menu itself, rather than the one of the
    // otherwise frontmost application window Hence we would need to store a variable somewhere that
    // contains the window ID of the last non-menu window... or is there a btter way?
    const auto &windows = KWindowSystem::windows();
    for (WId wid : windows) {
        KWindowSystem::minimizeWindow(wid);
    }
}

void AppMenuWidget::actionMaximizeAll()
{
    // TODO: In a similar way, implement "Hide <window name>" and "Hide others". For this we need to
    // know the window ID of the frontmost application window
    qDebug() << "probono: KWindowSystem::activeWindow;"
             << "0x" + QString::number(KWindowSystem::activeWindow(), 16);
    // NOTE: This always prints the window ID of the menu itself, rather than the one of the
    // otherwise frontmost application window Hence we would need to store a variable somewhere that
    // contains the window ID of the last non-menu window... or is there a btter way?
    const auto &windows = KWindowSystem::windows();
    for (WId wid : windows) {
        KWindowSystem::activateWindow(wid);
    }
}

void AppMenuWidget::actionLogout()
{
    qDebug() << "actionLogout() called";
    // Check if we have the Shutdown binary at hand
    if (QFileInfo(QCoreApplication::applicationDirPath() + QString("/Shutdown")).isExecutable()) {
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

    // TODO: Do the following without using notify-send. DBus is so complicated!
    QString program = "notify-send";
    QStringList arguments;
    arguments
            << tr("Force Quit Application")
            << tr("Click on the window of the application you would like to force quit. To cancel, "
                  "right-click.");

    QProcess::execute(program, arguments);

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

    if (!findProcess.waitForFinished())
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
// https://stackoverflow.com/a/3799197 (seems to require subclassing QMenu; we cant to avoid
// subclassing Qt)
bool AppMenuWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type()
        == QEvent::MouseButtonRelease) // here we are checking mouse button press event
    {
        qDebug() << __FILE__ ":" << __LINE__;
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        QMenu *submenu = qobject_cast<QMenu *>(
                watched); // Workaround for: no member named 'toolTip' in 'QObject'
        if (!submenu->rect().contains(
                    mouseEvent->pos())) { // Prevent the Menu action from getting triggred when user
                                          // click on actions in submenu
            // Gets executed when the submenu is clicked
            qDebug() << "Submenu clicked:" << submenu->property("path").toString();
            this->m_systemMenu->close(); // Could instead figure out the top-level menu iterating
                                         // through submenu->parent();
            QString pathToBeLaunched = submenu->property("path").toString();
            if (QFile::exists(pathToBeLaunched)) {
                QProcess::startDetached("open", { pathToBeLaunched });
            } else {
                qDebug() << pathToBeLaunched << "does not exist";
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    _layout = new QVBoxLayout;
    _imageLabel = new QLabel;
    _textLabel = new QLabel;
    _hardwareProbeButton = new QPushButton("Hardware Probe");
    _copyButton = new QPushButton("Copy to Clipboard");

    _textLabel->setContentsMargins(20, 0, 20, 0);

    _layout->setSizeConstraint(QLayout::SetFixedSize);
    _layout->addWidget(_imageLabel, 0, Qt::AlignHCenter);
    _layout->addWidget(_textLabel, 0, Qt::AlignHCenter);
    _layout->addWidget(_hardwareProbeButton, 0, Qt::AlignHCenter);
    _layout->addWidget(_copyButton, 0, Qt::AlignHCenter);

    // Insert spacing above and below the "Hardware Probe" button
    _layout->insertSpacing(2, 14);
    _layout->insertSpacing(-1, 10);

    connect(_hardwareProbeButton, &QPushButton::clicked, [=]() {
        QProcess::startDetached("launch", { "/Applications/Utilities/Hardware Probe.app" });
        accept();
    });
    _hardwareProbeButton->setEnabled(QFile::exists("/Applications/Utilities/Hardware Probe.app"));

    connect(_copyButton, &QPushButton::clicked, [=]() {
        qDebug() << "Copying to clipboard";
        QClipboard *clipboard = QApplication::clipboard();
        // TODO: Put together the text to be copied to the clipboard
        // in a more elegant way
        // Strip HTML tags
        QString text = _textLabel->text();
        text.replace("<br>", "\n");
        text.replace("</p>", "\n");
        text.replace("</h3>", "\n");
        text.remove(QRegularExpression("<[^>]*>"));
        // Remove all lines after the 9th
        QStringList lines = text.split("\n");
        if (lines.size() > 9) {
            lines = lines.mid(0, 9);
        }
        text = lines.join("\n");
        clipboard->setText(text);
        accept();
    });
    _copyButton->setEnabled(true);
    // Make the font size of the button 8pt, overriding stylesheet
    QFont font = _copyButton->font();
    font.setPointSize(7);
    _copyButton->setFont(font);

#if defined(Q_OS_FREEBSD)
    QProcess p;
    p.setProgram("kenv");
    p.setArguments({ "-q", "smbios.chassis.type" });
    p.start();
    p.waitForFinished();
    QString chassisType(p.readAllStandardOutput());
    chassisType.replace("\n", "");
    chassisType = chassisType.trimmed();
    qDebug() << "Chassis type:" << chassisType;

    QIcon icon = QIcon::fromTheme("computer");
    // https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.2.0.pdf#%5B%7B%22num%22%3A105%2C%22gen%22%3A0%7D%2C%7B%22name%22%3A%22XYZ%22%7D%2C70%2C555%2C0%5D

    // If chassis type is "Notebook" or "Laptop" or "Portable" or "Hand Held" or "Sub Notebook" or
    // "Convertible" then use "computer-laptop" icon
    if (chassisType == "Notebook" || chassisType == "Laptop" || chassisType == "Portable"
        || chassisType == "Hand Held" || chassisType == "Sub Notebook"
        || chassisType == "Convertible") {
        icon = QIcon::fromTheme("computer-laptop");
    }
#else
    QIcon icon = QIcon::fromTheme("computer");

#endif

    if (QApplication::keyboardModifiers()) {
        setWindowTitle(tr("About helloDesktop"));

        QPixmap pixmap = icon.pixmap(QSize(128, 128));
        _imageLabel->setPixmap(pixmap);
        _textLabel->setText(
                "<center><h3>helloDesktop</h3>"
                "<p>Lovingly crafted by true connoisseurs<br>of the desktop metaphor</p>"
                "<p>Inspired by the timeless vision<br>of Bill Atkinson and Andy Hertzfeld</p>"
                "<small>"
                "<p>Recommended reading: <a href='https://dl.acm.org/doi/book/10.5555/573097'>ISBN "
                "978-0-201-2216-4</a>"
                "</small></center>");
    } else {
        setWindowTitle(tr("About This Computer"));

        QString url;
        QString sha;
        QString build;

#if defined(Q_OS_FREEBSD)
        // Try to get extended attributes on the /.url file
        // TODO: We should use our cross-platform functions for this; we are using them
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
        p.setArguments({ "-q", "smbios.system.maker" });
        p.start();
        p.waitForFinished();
        QString vendorname(p.readAllStandardOutput());
        vendorname.replace("\n", "");
        vendorname = vendorname.trimmed();
        qDebug() << "vendorname:" << vendorname;

        p.setArguments({ "-q", "smbios.system.product" });
        p.start();
        p.waitForFinished();
        QString productname(p.readAllStandardOutput());
        productname.replace("\n", "");
        productname = productname.trimmed();
        qDebug() << "systemname:" << productname;

        p.setProgram("pkg");
        p.setArguments({ "info", "hello" });
        p.start();
        p.waitForFinished();
        QString operatingsystem(p.readAllStandardOutput());
        operatingsystem = operatingsystem.split("\n")[0].trimmed();
        if (operatingsystem != "") {
            // We are running on helloSystem
            operatingsystem =
                    operatingsystem.replace("hello-", "helloSystem ").replace("_", " (Build ")
                    + ")";
        } else {
            // We are not running on helloSystem (e.g., on FreeBSD + helloDesktop)
            operatingsystem = "helloDesktop (not running on helloSystem)";
        }

        p.setProgram("sysctl");
        p.setArguments({ "-n", "hw.model" });
        p.start();
        p.waitForFinished();
        QString cpu(p.readAllStandardOutput());
        cpu = cpu.trimmed();
        cpu = cpu.replace("(R)", "®");
        cpu = cpu.replace("(TM)", "™");
        qDebug() << "cpu:" << cpu;

        p.setArguments({ "-n", "hw.realmem" });
        p.start();
        p.waitForFinished();
        QString memory(p.readAllStandardOutput().trimmed());
        qDebug() << "memory:" << memory;
        float m = memory.toFloat();
        m = m / 1024 / 1024 / 1024;
        float roundedMem = round(m * 100.0) / 100.0; // Round to 2 digits
        qDebug() << "roundedMem:" << roundedMem;

        p.setProgram("freebsd-version");
        p.setArguments({ "-k" });
        p.start();
        p.waitForFinished();
        QString kernelVersion(p.readAllStandardOutput());

        p.setArguments({ "-u" });
        p.start();
        p.waitForFinished();
        QString userlandVersion(p.readAllStandardOutput());

        // See
        // https://github.com/openwebos/qt/blob/92fde5feca3d792dfd775348ca59127204ab4ac0/tools/qdbus/qdbusviewer/qdbusviewer.cpp#L477
        // for loading icon from resources
        QString helloSystemInfo;
        if (sha != "" && url != "" && build != "") {
            helloSystemInfo = "</p>helloSystem build: " + build + " for commit: <a href='" + url
                    + "'>" + sha + "</a></p>";
        } else if (sha != "" && url != "") {
            helloSystemInfo = "</p>helloSystem commit: <a href='" + url + "'>" + sha + "</a></p>";
        }

        QString gpu = tr("Unknown");
        // The following works on FreeBSD with initgfx
        QFile file("/var/log/Xorg.0.log");
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.contains("-->Device")) {
                    gpu = line.split('"')[1];
                }
            }
            file.close();
        }

        QPixmap pixmap = icon.pixmap(QSize(128, 128)); // create a QPixmap from the QIcon
        _imageLabel->setPixmap(pixmap);
        _textLabel->setText(
                "<center><h3>" + vendorname + " " + productname + "</h3>" + "<p>" + operatingsystem
                + "</p><small>" + "<p>FreeBSD kernel version: " + kernelVersion + "<br>"
                + "FreeBSD userland version: " + userlandVersion + "</p>" + "<p>Processor: " + cpu
                + "<br>" + "Graphics: " + gpu + "<br>" + "Memory: " + QString::number(roundedMem)
                + " GiB<br>" + helloSystemInfo
                + "<p><a href='file:///COPYRIGHT'>FreeBSD copyright information</a><br>"
                + "Other components are subject to<br>their respective license terms</p>"
                + "</small></center>");
#else
        _textLabel->setText(
                QString("<center><h3>helloDesktop</h3>"
                        "<p>Running on an unsupported operating system<br>"
                        "with reduced functionality</p>"
                        "<small><p>The full desktop experience<br>"
                        "can best be experienced on helloSystem<br>"
                        "which helloDesktop is designed for</p>"
                        ""
                        "<a href='https://hellosystem.github.io'>https://hellosystem.github.io/</a>"
                        "</small></center>"));
#endif
    }

    setAttribute(Qt::WA_DeleteOnClose);
    setLayout(_layout);
    setModal(false);
    setSizeGripEnabled(false);
}

AboutDialog::~AboutDialog() { }
