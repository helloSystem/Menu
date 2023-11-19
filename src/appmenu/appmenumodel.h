/******************************************************************
 * Copyright 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ******************************************************************/

#ifndef APPMENUMODEL_H
#define APPMENUMODEL_H
#include <chrono>
#include <QAbstractListModel>
#include <QAbstractNativeEventFilter>
#include <QStringList>
#include <KWindowSystem>
#include <QPointer>
#include <QRect>
#include <QDebug>
#include <QTimer>
#include <dbusmenu-qt5/dbusmenuimporter.h>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QActionEvent>
#include <QApplication>
class QAction;
class QModelIndex;
class QDBusServiceWatcher;
class AppMenuModel;
class MainWindow;

class HMenu : public QMenu
{

    Q_OBJECT

public:
    HMenu(QWidget *parent = 0) : QMenu(parent) { m_locale_lang = QLocale().language(); }
    std::chrono::high_resolution_clock::time_point lastOpened;

protected:
    /**
     * Handles the given `QActionEvent` for the `HMenu`.
     *
     * This event handler handles `QEvent::ActionAdded` and `QEvent::ActionRemoved`
     * events. For `QEvent::ActionAdded` events, it removes mnemonics (underlined
     * characters in menus represented by "&" in code), adds the action's menu to
     * the menu bar if the menu's parent is a `QMenuBar`, and translates the action's
     * text to "Ablage" if it is "Datei" in German. For `QEvent::ActionRemoved`
     * events, it removes the action from the menu bar if the menu's parent is a
     * `QMenuBar`. Finally, the handler passes the event on to the base class
     * implementation of `actionEvent` in `QMenu`.
     *
     * @param e The `QActionEvent` to handle.
     */
    void actionEvent(QActionEvent *e) override;

private:
    QLocale::Language m_locale_lang;
};

class HDBusMenuImporter : public DBusMenuImporter
{
public:
    HDBusMenuImporter(const QString &service, const QString &path,
                      const enum DBusMenuImporterType type, QObject *parent = 0)
        : DBusMenuImporter(service, path, type, parent)

    {
        m_reshowTimer = new QTimer();
        connect(m_reshowTimer, &QTimer::timeout, this, [this] {
            if (recent) {
                recent->blockSignals(true);
                qDebug() << "reshow" << __LINE__ << recent->title();
                qobject_cast<QMenuBar *>(recent->parent()->parent())
                        ->setActiveAction(recent->menuAction());
                recent->blockSignals(false);
                recent = nullptr;
            }
        });
    }

    /* Workaround for e.g., Falkon History menu */
    QMenu *createMenu(QWidget *parent) override
    {
        HMenu *menu = new HMenu(parent);
        // Make some workarounds for focus loss which  calls closeAllPopups();
        if (parent && qobject_cast<QMenuBar *>(parent->parent())) {
            connect(menu, &QMenu::aboutToShow, this, [this] {
                recent = qobject_cast<HMenu *>(sender());
                qobject_cast<HMenu *>(sender())->lastOpened =
                        std::chrono::high_resolution_clock::now();
                m_reshowTimer->stop();
            });
            connect(menu, &QMenu::aboutToHide, this, [this] {
                HMenu *reshow = qobject_cast<HMenu *>(sender());

                m_reshowTimer->setSingleShot(true);
                m_reshowTimer->setInterval(100); // if you show another menu within 100ms the reshow
                                                 // action is canceled.
                std::chrono::duration<double, std::milli> dur =
                        std::chrono::high_resolution_clock::now() - reshow->lastOpened;

                if (dur.count() < 350) // start reshow timer on fastly reclosed menus
                    m_reshowTimer->start();
            });
        }
        return menu;
    }

public:
    QTimer *m_reshowTimer;
    HMenu *recent;
};

class AppMenuModel : public QAbstractItemModel, public QAbstractNativeEventFilter
{
    Q_OBJECT

    Q_PROPERTY(bool menuAvailable READ menuAvailable WRITE setMenuAvailable NOTIFY
                       menuAvailableChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)

    Q_PROPERTY(bool filterByActive READ filterByActive WRITE setFilterByActive NOTIFY
                       filterByActiveChanged)
    Q_PROPERTY(bool filterChildren READ filterChildren WRITE setFilterChildren NOTIFY
                       filterChildrenChanged)

    Q_PROPERTY(QRect screenGeometry READ screenGeometry WRITE setScreenGeometry NOTIFY
                       screenGeometryChanged)

    Q_PROPERTY(QVariant winId READ winId WRITE setWinId NOTIFY winIdChanged)
public:
    /**
     * Constructs a new `AppMenuModel` with the given `parent` object.
     *
     * The `AppMenuModel` provides a QAbstractItemModel implementation for
     * an application's menu, allowing it to be used in a QMenuBar or QMenu.
     *
     * The model uses the DBusMenu protocol to communicate with the application's
     * DBusMenu service and retrieve the menu structure and items. It also
     * handles changes to the active window and updates the menu accordingly.
     *
     * @param parent The parent object of the `AppMenuModel`.
     */
    explicit AppMenuModel(QObject *parent = nullptr);

    /**
     * Destroys the `AppMenuModel` object.
     */
    ~AppMenuModel() override;

    enum AppMenuRole {
        MenuRole = Qt::UserRole + 1, // TODO this should be Qt::DisplayRole
        ActionRole
    };
    QHash<QString, bool> m_pending_service;

    QAction *findParent(QAction *child, QAction *root) const;
    /**
     * Returns the data for the given model index and role.
     *
     * This method returns the data for the given model index and role,
     * based on the contents of the menu being represented by the model.
     * If the index is not valid or the menu is null, an empty QVariant
     * will be returned. Otherwise, the data for the corresponding action
     * will be returned, depending on the role.
     *
     * @param index The model index for which to return the data.
     * @param role The role for which to return the data.
     * @return The data for the given model index and role.
     */
    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    QHash<int, QByteArray> roleNames() const override;

    /**
     * Returns the model index of the item at the specified `row` and `column`
     * under the given `parent` index.
     *
     * @param row The row of the item for which to return the index.
     * @param column The column of the item for which to return the index.
     * @param parent The parent index of the item for which to return the index.
     *
     * @return The model index of the item, or an invalid index if the item
     *         does not exist.
     */
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &index) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

    QMap<QString, QAction *> filteredActions() { return m_visibleActions; }

    /**
     * Updates the application menu with the given service name and menu object path.
     *
     * This method updates the application menu with the menu provided by the
     * D-Bus service with the given service name and menu object path. It does
     * this by removing any existing actions from the menu bar, creating a new
     * D-Bus menu importer for the given service and menu object path, and
     * setting the menu from the importer as the current menu for the model.
     * The `menuAboutToBeImported` and `menuImported` signals are emitted
     * before and after the menu is imported, respectively.
     *
     * @param serviceName The D-Bus service name of the menu to import.
     * @param menuObjectPath The D-Bus object path of the menu to import.
     */
    void updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath);

    void updateSearch();

    void invalidateMenu() { m_menu = nullptr; }

    bool filterByActive() const;

    void setFilterByActive(bool active);

    // void execute(QString actionName);

    bool filterChildren() const;

    void setFilterChildren(bool hideChildren);

    bool menuAvailable() const;

    void setMenuAvailable(bool set);

    void clearFilteredActions() { m_visibleActions.clear(); }

    /**
     * Filters the given menu based on the given search string.
     *
     * This method filters the actions of the given menu and its submenus,
     * based on the given search string. If the search string is empty, all
     * actions will be shown. If the search string is not empty, only actions
     * whose names contain the search string (case-insensitive) will be shown.
     * The visibility of the menu itself will also be updated based on the
     * search string and the includeDisabled flag.
     *
     * @param searchMenu The menu to filter.
     * @param searchString The search string to use for filtering the menu.
     * @param includeDisabled Whether to include disabled actions in the search results.
     * @param names The list of names to which the action names will be added.
     * @return Whether any visible actions were found in the menu.
     */
    bool filterMenu(QMenu *searchMenu, QString searchString, bool includeDisabled,
                    QStringList names);

    bool visible() const;

    QList<QAction *> mActions() { return m_actions; }

    QRect screenGeometry() const;

    void setScreenGeometry(QRect geometry);

    QVariant winId() const;

    void setWinId(const QVariant &id);

    /**
     * Reads the actions in the given menu and adds them to the list of names.
     *
     * This method recursively traverses the menu hierarchy starting from the
     * given menu, and for each action in the menu, it adds the action's text
     * to the list of names. If the action has a sub-menu, this method is called
     * again with the sub-menu as the argument.
     *
     * The names are joined with the "▸" separator and added to the `names`
     * parameter, which is passed by reference.
     *
     * Additionally, if an action does not have a sub-menu and is enabled,
     * its name is added to the `m_names` property (a map of action names
     * to actions).
     *
     * @param menu The menu to read actions from.
     * @param names The list of names to add the action names to.
     */
    void readMenuActions(QMenu *menu, QStringList names);

    QPointer<QMenu> menu() { return m_menu; }

    void refreshSearch();

signals:
    void requestActivateIndex(int index);

    void menuAboutToBeImported();

protected:
    /**
     * Filters native (X11) events for the application menu.
     *
     * This method filters native (X11) events for the application menu by handling
     * property change events for the current active window. If the property
     * change event corresponds to the service name or object path of the
     * application menu, the `onActiveWindowChanged` method is called to
     * update the menu.
     *
     * @param eventType The type of the native event.
     * @param message The native event data.
     * @param result A pointer to the result of the event filter.
     * @return `true` if the event was handled by this filter, `false` otherwise.
     */
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;

public Q_SLOTS:

private Q_SLOTS:
    /**
     * Handles a change in the active window with the given ID.
     *
     * This method checks if the active window is the same as the parent widget of
     * the `AppMenuModel`, and if so, it retrieves and updates the application menu
     * for the active window. If the active window is not the parent widget, it
     * checks if the `AppMenuModel` has been associated with a specific window
     * (via the `winId` property), and if so, it ignores the change in active
     * window. Otherwise, it sets the `menuAvailable` property to `false` and
     * triggers an update of the model.
     *
     * @param id The ID of the active window.
     */
    void onActiveWindowChanged(WId id);

    void onWindowChanged(WId id);

    //! there are apps that are not releasing their menu properly after closing
    //! and as such their menu is still shown even though the app does not exist
    //! anymore. Such apps are Java based e.g. smartgit
    void onWindowRemoved(WId id);

    void filterWindow(KWindowInfo &info);

    void setVisible(bool visible);

    void update();

signals:
    void menuAvailableChanged();
    void modelNeedsUpdate();
    void menuImported(QString serviceName);
    void firstLevelParsed();
    void filterByActiveChanged();
    void filterChildrenChanged();
    void visibleChanged();
    void screenGeometryChanged();
    void winIdChanged();

private:
    void deleteActions(QMenu *menu());
    QWidget *w_parent;
    QMap<QString, QAction *> m_names;
    bool m_filterByActive = false;
    bool m_filterChildren = false;
    bool m_menuAvailable;
    bool m_updatePending = false;
    bool m_visible = true;
    bool m_awaitsUpdate;
    QRect m_screenGeometry;
    bool hasVisible = false;
    QMap<QString, QAction *> m_visibleActions;
    QVariant m_winId{ -1 };
    //! current active window used
    WId m_currentWindowId = 0;
    WId m_initialApplicationFromWindowId = -1;
    //! window that its menu initialization may be delayed
    WId m_delayedMenuWindowId = 0;
    QPointer<QMenu> m_menu;
    QList<QAction *> m_actions;
    QDBusServiceWatcher *m_serviceWatcher;
    QString m_serviceName;
    QString m_menuObjectPath;
    bool m_refreshSearch;
    DBusMenuImporter *m_importer;
    QHash<QString, DBusMenuImporter *> m_importers;
};

#endif
