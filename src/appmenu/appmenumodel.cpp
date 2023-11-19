/******************************************************************
 * Copyright 2016 Kai Uwe Broulik <kde@privat.broulik.de>
 * Copyright 2016 Chinmoy Ranjan Pradhan <chinmoyrp65@gmail.com>
 *
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

/*
 * This class provides an implementation of a QAbstractItemModel for an application's menu.
 * The model can be used in a QMenuBar or a QMenu to display the application's menu.
 * The class uses the DBusMenu protocol to communicate with the application's DBusMenu service
 * and retrieve the menu structure and items.
 * It also handles changes to the active window and updates the menu accordingly.
 */

#include "appmenumodel.h"

#include <QX11Info>
#include <xcb/xcb.h>
#include <QAction>
#include <QMenu>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusServiceWatcher>
#include <QGuiApplication>
#include <QTimer>
#include <QMutex>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QDebug>
#include <QModelIndex>
#include <dbusmenu-qt5/dbusmenuimporter.h>

QMutex mutex;
static const QByteArray s_x11AppMenuServiceNamePropertyName =
        QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME");
static const QByteArray s_x11AppMenuObjectPathPropertyName =
        QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH");

static QHash<QByteArray, xcb_atom_t> s_atoms;

void HMenu::actionEvent(QActionEvent *e)
{
    if (e->type() == QEvent::ActionAdded) {
        // probono: Remove mnemonics (underlined characters in menus represented by "&" in code)
        // QtPlugin doesn't show the underlines, but they are still functional unless we remove them
        e->action()->setText(e->action()->text().replace("&", "")); // probono: Remove mnemonics
        if (qobject_cast<QMenuBar *>(parent()) != nullptr) { // only happens for first level
            if (e->action()->menu())
                qobject_cast<QMenuBar *>(parent())->addMenu(e->action()->menu());
            if (m_locale_lang == QLocale::German) {
                if (e->action()->text().replace("&", "") == "Datei") {
                    e->action()->setText("Ablage");
                }
            }
        }
    }
    if (e->type() == QEvent::ActionRemoved) {
        if (qobject_cast<QMenuBar *>(parent()) != nullptr) {
            qobject_cast<QMenuBar *>(parent())->removeAction(e->action());
        }
    }
    QMenu::actionEvent(e);
}

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractItemModel(parent), m_serviceWatcher(new QDBusServiceWatcher(this))
{
    w_parent = qobject_cast<QWidget *>(parent);
    if (!KWindowSystem::isPlatformX11()) {
        return;
    }

    connect(this, &AppMenuModel::winIdChanged, this,
            [this] { onActiveWindowChanged(m_winId.toUInt()); });

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this,
            &AppMenuModel::onActiveWindowChanged);
    connect(KWindowSystem::self(),
            static_cast<void (KWindowSystem::*)(WId)>(&KWindowSystem::windowChanged), this,
            &AppMenuModel::onWindowChanged);
    connect(KWindowSystem::self(),
            static_cast<void (KWindowSystem::*)(WId)>(&KWindowSystem::windowRemoved), this,
            &AppMenuModel::onWindowRemoved);

    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending) {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
    });

    connect(this, &AppMenuModel::screenGeometryChanged, this,
            [this] { onWindowChanged(m_currentWindowId); });

    onActiveWindowChanged(KWindowSystem::activeWindow());

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    // if our current DBus connection gets lost, close the menu
    // we'll select the new menu when the focus changes
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this,
            [this](const QString &serviceName) {
                if (serviceName == m_serviceName) {
                    setMenuAvailable(false);
                    emit modelNeedsUpdate();
                }
            });
}

AppMenuModel::~AppMenuModel() = default;

QModelIndex AppMenuModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }

    QAction *parentItem;
    parentItem = static_cast<QAction *>(parent.internalPointer());

    if (!parent.isValid()) {
        parentItem = m_menu->menuAction();
    }
    if (row >= 0 && parentItem->menu() && row < parentItem->menu()->actions().count()) {
        return createIndex(row, column, parentItem->menu()->actions().at(row));
    } else
        return QModelIndex();
}

QAction *AppMenuModel::findParent(QAction *child, QAction *root) const
{

    QAction *menu = qobject_cast<QMenu *>(child->parent())->menuAction();

    return menu;
}
int AppMenuModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QModelIndex AppMenuModel::parent(const QModelIndex &index) const
{

    if (!index.isValid())
        return QModelIndex();
    int row;
    QAction *item = static_cast<QAction *>(index.internalPointer());
    QAction *p = findParent(item, m_menu->menuAction());
    if (p == m_menu->menuAction()) {

        return QModelIndex();
    }

    QAction *gp = findParent(p, m_menu->menuAction());

    row = gp->menu()->actions().indexOf(p);

    return createIndex(row, 0, p);
}

bool AppMenuModel::filterByActive() const
{
    return m_filterByActive;
}

void AppMenuModel::setFilterByActive(bool active)
{
    if (m_filterByActive == active) {
        return;
    }

    m_filterByActive = active;
    emit filterByActiveChanged();
}

bool AppMenuModel::filterChildren() const
{
    return m_filterChildren;
}

void AppMenuModel::setFilterChildren(bool hideChildren)
{
    if (m_filterChildren == hideChildren) {
        return;
    }

    m_filterChildren = hideChildren;
    emit filterChildrenChanged();
}

bool AppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

void AppMenuModel::setMenuAvailable(bool set)
{

    if (m_menuAvailable != set) {
        m_menuAvailable = set;
        // onWindowChanged(m_currentWindowId);
        emit menuAvailableChanged();
    }
}

QRect AppMenuModel::screenGeometry() const
{
    return m_screenGeometry;
}

void AppMenuModel::setScreenGeometry(QRect geometry)
{
    if (m_screenGeometry == geometry) {
        return;
    }

    m_screenGeometry = geometry;
    emit screenGeometryChanged();
}

bool AppMenuModel::visible() const
{
    return m_visible;
}

void AppMenuModel::setVisible(bool visible)
{
    if (m_visible != visible) {
        m_visible = visible;
        emit visibleChanged();
    }
}

QVariant AppMenuModel::winId() const
{
    return m_winId;
}

void AppMenuModel::setWinId(const QVariant &id)
{

    if (m_winId == id) {
        return;
    }

    m_winId = id;
    emit winIdChanged();
}

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    if (!m_menu) {
        return 0;
    }

    QAction *parentItem;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parentItem = m_menu->menuAction();
    else
        parentItem = static_cast<QAction *>(parent.internalPointer());

    if (parentItem->menu()) {

        return parentItem->menu()->actions().count();
    }
    return 0;
}

void AppMenuModel::update()
{
    beginResetModel();
    endResetModel();
    m_updatePending = false;
}

void AppMenuModel::onActiveWindowChanged(WId id)
{
    if (id == 0)
        return;

    qApp->removeNativeEventFilter(this);

    auto pw = qobject_cast<QWidget *>(w_parent);

    if (pw) {
        if (id == pw->effectiveWinId()) {
            /// Get more options for search
            if (m_initialApplicationFromWindowId == -1) {
                return;
            }

            if (KWindowSystem::isPlatformX11()) {
                auto *c = QX11Info::connection();

                auto getWindowPropertyString = [c, this](WId id,
                                                         const QByteArray &name) -> QByteArray {
                    QByteArray value;

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
                            return value;
                        }
                    }

                    static const long MAX_PROP_SIZE = 10000;
                    auto propertyCookie = xcb_get_property(c, false, id, s_atoms[name],
                                                           XCB_ATOM_STRING, 0, MAX_PROP_SIZE);
                    QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter>
                            propertyReply(xcb_get_property_reply(c, propertyCookie, nullptr));

                    if (propertyReply.isNull()) {
                        return value;
                    }

                    if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8
                        && propertyReply->value_len > 0) {
                        const char *data =
                                (const char *)xcb_get_property_value(propertyReply.data());
                        int len = propertyReply->value_len;

                        if (data) {
                            value = QByteArray(data, data[len - 1] ? len : len - 1);
                        }
                    }

                    return value;
                };

                const QString serviceName = QString::fromUtf8(getWindowPropertyString(
                        m_initialApplicationFromWindowId, s_x11AppMenuServiceNamePropertyName));

                const QString menuObjectPath = QString::fromUtf8(getWindowPropertyString(
                        m_initialApplicationFromWindowId, s_x11AppMenuObjectPathPropertyName));

                if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
                    m_pending_service[m_serviceName] = true;
                    updateApplicationMenu(m_serviceName, m_menuObjectPath);
                } else {
                    m_initialApplicationFromWindowId = -1;
                }
            } else {
                m_initialApplicationFromWindowId = -1;
            }
            return;
        }
    }

    if (m_winId != -1 && m_winId != id) {
        //! ignore any other window except the one preferred from plasmoid
        return;
    }

    if (!id) {
        setMenuAvailable(false);
        emit modelNeedsUpdate();
        return;
    }

    if (KWindowSystem::isPlatformX11()) {
        auto *c = QX11Info::connection();

        auto getWindowPropertyString = [c, this](WId id, const QByteArray &name) -> QByteArray {
            QByteArray value;

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
                    return value;
                }
            }

            static const long MAX_PROP_SIZE = 10000;
            auto propertyCookie = xcb_get_property(c, false, id, s_atoms[name], XCB_ATOM_STRING, 0,
                                                   MAX_PROP_SIZE);
            QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> propertyReply(
                    xcb_get_property_reply(c, propertyCookie, nullptr));

            if (propertyReply.isNull()) {
                return value;
            }

            if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8
                && propertyReply->value_len > 0) {
                const char *data = (const char *)xcb_get_property_value(propertyReply.data());
                int len = propertyReply->value_len;

                if (data) {
                    value = QByteArray(data, data[len - 1] ? len : len - 1);
                }
            }

            return value;
        };

        auto updateMenuFromWindowIfHasMenu = [this, &getWindowPropertyString](WId id) {
            const QString serviceName = QString::fromUtf8(
                    getWindowPropertyString(id, s_x11AppMenuServiceNamePropertyName));
            const QString menuObjectPath = QString::fromUtf8(
                    getWindowPropertyString(id, s_x11AppMenuObjectPathPropertyName));

            qDebug() << "probono: WM_CLASS"
                     << QString::fromUtf8(
                                getWindowPropertyString(id, QByteArrayLiteral("WM_CLASS")));
            qDebug() << serviceName << menuObjectPath << __LINE__ << id;
            if (!serviceName.isEmpty() && !menuObjectPath.isEmpty()) {
                m_initialApplicationFromWindowId = id;
                mutex.lock();
                m_pending_service[m_serviceName] = true;
                updateApplicationMenu(serviceName, menuObjectPath);
                mutex.unlock();
                return true;
            }

            return false;
        };

        KWindowInfo info(id, NET::WMState | NET::WMWindowType | NET::WMGeometry,
                         NET::WM2TransientFor);

        if (info.hasState(NET::SkipTaskbar) || info.windowType(NET::UtilityMask) == NET::Utility) {

            //! hide when the windows or their transient(s) do not have a menu
            if (filterByActive()) {

                KWindowInfo transientInfo = KWindowInfo(
                        info.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry,
                        NET::WM2TransientFor);

                while (transientInfo.win()) {
                    if (transientInfo.win() == m_currentWindowId) {
                        filterWindow(info);
                        return;
                    }

                    transientInfo = KWindowInfo(transientInfo.transientFor(),
                                                NET::WMState | NET::WMWindowType | NET::WMGeometry,
                                                NET::WM2TransientFor);
                }
            }

            if (filterByActive()) {
                setVisible(false);
            }

            // rekols: 切换到桌面时要隐藏menubar
            setMenuAvailable(false);
            emit modelNeedsUpdate();

            return;
        }

        m_currentWindowId = id;

        if (!filterChildren()) {
            KWindowInfo transientInfo = KWindowInfo(
                    info.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry,
                    NET::WM2TransientFor);

            // look at transient windows first
            while (transientInfo.win()) {
                if (updateMenuFromWindowIfHasMenu(transientInfo.win())) {
                    filterWindow(info);
                    return;
                }

                transientInfo = KWindowInfo(transientInfo.transientFor(),
                                            NET::WMState | NET::WMWindowType | NET::WMGeometry,
                                            NET::WM2TransientFor);
            }
        }

        if (updateMenuFromWindowIfHasMenu(id)) {
            filterWindow(info);
            return;
        }

        // monitor whether an app menu becomes available later
        // this can happen when an app starts, shows its window, and only later announces global
        // menu (e.g. Firefox)
        qApp->installNativeEventFilter(this);
        m_delayedMenuWindowId = id;

        // no menu found, set it to unavailable

        setMenuAvailable(false);
        emit modelNeedsUpdate();
    }
}

bool AppMenuModel::hasChildren(const QModelIndex &parent) const
{
    QAction *parentItem;
    if (!parent.isValid())
        return true;

    parentItem = static_cast<QAction *>(parent.internalPointer());

    return (bool)(parentItem->menu());
}
void AppMenuModel::onWindowChanged(WId id)
{
    if (m_currentWindowId == id) {
        KWindowInfo info(id, NET::WMState | NET::WMGeometry);
        filterWindow(info);
    }
}

void AppMenuModel::onWindowRemoved(WId id)
{
    if (m_currentWindowId == id) {
        setMenuAvailable(false);
        setVisible(false);
    }
}

void AppMenuModel::filterWindow(KWindowInfo &info)
{
    if (m_currentWindowId == info.win()) {
        //! HACK: if the user has enabled screen scaling under X11 environment
        //! then the window and screen geometries cannot be trusted for comparison
        //! before windows coordinates be adjusted properly.
        //! BUG: 404500
        QPoint windowCenter = info.geometry().center();
        if (KWindowSystem::isPlatformX11()) {
            windowCenter /= qApp->devicePixelRatio();
        }
        const bool contained = m_screenGeometry.isNull() || m_screenGeometry.contains(windowCenter);

        const bool isActive = m_filterByActive ? info.win() == KWindowSystem::activeWindow() : true;

        setVisible(isActive && !info.isMinimized() && contained);
    }
}
QHash<int, QByteArray> AppMenuModel::roleNames() const
{
    QHash<int, QByteArray> roleNames;
    roleNames[MenuRole] = QByteArrayLiteral("activeMenu");
    roleNames[ActionRole] = QByteArrayLiteral("activeActions");
    return roleNames;
}

bool AppMenuModel::filterMenu(QMenu *searchMenu, QString searchString, bool includeDisabled,
                              QStringList names)
{

    if (!searchMenu) {
        return false;
    }
    searchMenu->menuAction()->setVisible(searchString == "");

    if (!searchMenu->title().isEmpty())
        names << searchMenu->title();

    const QList<QAction *> actions = searchMenu->actions();
    for (QAction *action : actions) {
        action->setVisible(searchString == "");

        if (action->menu()) {
            if (searchString != "" && action->text().contains(searchString, Qt::CaseInsensitive)) {
                action->setVisible(true);

                QAction *parent = qobject_cast<QMenu *>(action->parent())->menuAction();

                while (parent) {
                    parent->setVisible(true);
                    parent->setVisible(!parent->isSeparator());
                    if (parent->menu() && parent->menu()->parent()) {

                        QMenu *p = qobject_cast<QMenu *>(parent->menu()->parent());
                        if (!p)
                            break;
                        parent = p->menuAction();
                    } else {
                        break;
                    }
                }

            } else {
                filterMenu(action->menu(), searchString, searchString == "", names);
            }

        } else {
            if (searchString == "") {
                hasVisible = true;
                action->setVisible(true);
                if (!includeDisabled) {
                    action->setVisible(action->isEnabled());
                    ;
                    hasVisible = action->isEnabled();
                }
            }
            if (!searchString.isEmpty()
                && action->text().contains(searchString, Qt::CaseInsensitive)) {

                action->setVisible(true);
                // hasVisible = !action->isSeparator() && action->isEnabled();
                hasVisible =
                        !action->isSeparator(); // FIXME:
                                                // https://github.com/helloSystem/Menu/issues/99#issuecomment-1281166629

                action->setVisible(hasVisible);

                if (hasVisible) {
                    QAction *parent = nullptr;
                    QMenu *parentMenu = qobject_cast<QMenu *>(action->parent());
                    if (parentMenu) {
                        parent = parentMenu->menuAction();
                    }
                    while (parent) {
                        parent->setVisible(!parent->isSeparator());
                        if (parent->menu() && parent->menu()->parent()) {

                            QMenu *p = qobject_cast<QMenu *>(parent->menu()->parent());
                            if (!p)
                                break;
                            parent = p->menuAction();

                        } else {
                            break;
                        }
                    }

                    m_visibleActions[names.join(" ▸ ") + " ▸ " + action->text()] = action;
                }
            } else if (qobject_cast<QWidgetAction *>(action) != nullptr
                       && action->text() == QString("")) {

                action->setVisible(true);

            } else if (!searchString.isEmpty()) {
                action->setVisible(false);
                hasVisible = false;
            }
        }
    }

    names.clear();
    return hasVisible;
}

void AppMenuModel::readMenuActions(QMenu *menu, QStringList names)
{
    // See https://doc.qt.io/qt-5/qaction.html#menu
    // If a QAction does not have menu in it, then
    // the pointer returned by QAction::menu will be
    // null.
    if (!menu)
        return;
    if (!menu->title().isEmpty())
        names << menu->title();
    const QList<QAction *> actions = menu->actions();
    for (auto action : actions) {
        if (action->menu() != NULL) {

            readMenuActions(action->menu(), names);
        }

        QString actionName = action->text().replace("&", "");
        if (!actionName.isEmpty() && action->isEnabled() && action->menu() == NULL) {

            actionName = names.join(" ▸ ") + " ▸ " + actionName;
            m_names.insert(actionName, action);
        }
    }
}

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    if (m_menu.isNull()) {

        return QVariant();
    }

    if (!index.isValid()) {

        if (role == Qt::DisplayRole) {
            return QVariant::fromValue(m_menu->menuAction());
        }
    }

    const int row = index.row();
    /*if (row == actions.count() && m_searchAction) {
        if (role == MenuRole) {
            return m_searchAction->text();
        } else if (role == ActionRole) {
            return QVariant::fromValue(m_searchAction.data());
        }
    }*/

    ;
    if (role == Qt::DisplayRole) {

        return QVariant::fromValue(static_cast<QAction *>(index.internalPointer()));
    }
    if (role == Qt::EditRole) {

        return QVariant::fromValue(static_cast<QAction *>(index.internalPointer()));
    }
    return QVariant();
}

/*
void AppMenuModel::execute(QString actionName)
{
    if (m_names.keys().contains(actionName)) {
        m_names[actionName]->trigger();
    }
}*/

void AppMenuModel::refreshSearch() { }

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    if (m_pending_service[serviceName] && m_serviceName == serviceName) {
        qDebug() << "Skipping" << serviceName << menuObjectPath << "switching too fast";
    }
    QMenuBar *menuBar = qobject_cast<QMenuBar *>(
            w_parent); /*m_importers[serviceName+menuObjectPath]->menu()->parent());*/
    int cnt = menuBar->actions().count();
    QList<QAction *> remove;
    for (int i = 2; i < cnt; i++)
        remove.append(menuBar->actions().at(i));
    for (QAction *r : remove) {
        menuBar->removeAction(r);
    }
    if (m_importers[serviceName + menuObjectPath]) {
        m_importers[serviceName + menuObjectPath]->deleteLater();
    }

    m_serviceName = serviceName;
    m_menuObjectPath = menuObjectPath;
    m_serviceWatcher->setWatchedServices(QStringList({ m_serviceName }));
    emit menuAboutToBeImported();

    HDBusMenuImporter *importer =
            new HDBusMenuImporter(serviceName, menuObjectPath, DBusMenuImporterType::SYNCHRONOUS);

    m_importers[serviceName + menuObjectPath] = importer;
    m_importers[serviceName + menuObjectPath]->menu()->setParent(w_parent);
    m_menu = m_importers[serviceName + menuObjectPath]->menu();

    setMenuAvailable(!m_menu.isNull());

    emit menuImported(serviceName);
}

void AppMenuModel::updateSearch() { }

bool AppMenuModel::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
{
    Q_UNUSED(result);

    if (!KWindowSystem::isPlatformX11() || eventType != "xcb_generic_event_t") {
        return false;
    }

    auto e = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = e->response_type & ~0x80;

    if (type == XCB_PROPERTY_NOTIFY) {
        auto *event = reinterpret_cast<xcb_property_notify_event_t *>(e);

        if (event->window == m_delayedMenuWindowId) {

            auto serviceNameAtom = s_atoms.value(s_x11AppMenuServiceNamePropertyName);
            auto objectPathAtom = s_atoms.value(s_x11AppMenuObjectPathPropertyName);

            if (serviceNameAtom != XCB_ATOM_NONE
                && objectPathAtom != XCB_ATOM_NONE) { // shouldn't happen
                if (event->atom == serviceNameAtom || event->atom == objectPathAtom) {
                    // see if we now have a menu
                    onActiveWindowChanged(KWindowSystem::activeWindow());
                }
            }
        }
    }

    return false;
}
