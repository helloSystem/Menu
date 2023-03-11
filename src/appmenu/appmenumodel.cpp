
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
#include <QDebug>

#include "dbusmenuimporter.h"

static const QByteArray s_x11AppMenuServiceNamePropertyName = QByteArrayLiteral("_KDE_NET_WM_APPMENU_SERVICE_NAME");
static const QByteArray s_x11AppMenuObjectPathPropertyName = QByteArrayLiteral("_KDE_NET_WM_APPMENU_OBJECT_PATH");

static QHash<QByteArray, xcb_atom_t> s_atoms;

class KDBusMenuImporter : public DBusMenuImporter
{
public:
    KDBusMenuImporter(const QString &service, const QString &path, QObject *parent)
        : DBusMenuImporter(service, path, parent) {
    }

protected:
    QIcon iconForName(const QString &name) override {
        return QIcon::fromTheme(name);
    }
};

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent),
      m_serviceWatcher(new QDBusServiceWatcher(this))
{
    if (!KWindowSystem::isPlatformX11()) {
        return;
    }

    connect(this, &AppMenuModel::winIdChanged, this, [this] {
        onActiveWindowChanged(m_winId.toUInt());
    });

    connect(KWindowSystem::self(), &KWindowSystem::activeWindowChanged, this, &AppMenuModel::onActiveWindowChanged);
    connect(KWindowSystem::self()
            , static_cast<void (KWindowSystem::*)(WId)>(&KWindowSystem::windowChanged)
            , this
            , &AppMenuModel::onWindowChanged);
    connect(KWindowSystem::self()
            , static_cast<void (KWindowSystem::*)(WId)>(&KWindowSystem::windowRemoved)
            , this
            , &AppMenuModel::onWindowRemoved);

    connect(this, &AppMenuModel::modelNeedsUpdate, this, [this] {
        if (!m_updatePending)
        {
            m_updatePending = true;
            QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
        }
    });

    connect(this, &AppMenuModel::screenGeometryChanged, this, [this] {
        onWindowChanged(m_currentWindowId);
    });

    onActiveWindowChanged(KWindowSystem::activeWindow());

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    //if our current DBus connection gets lost, close the menu
    //we'll select the new menu when the focus changes
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString & serviceName) {
        if (serviceName == m_serviceName) {
            setMenuAvailable(false);
            emit modelNeedsUpdate();
            emit menuParsed();
        }
    });
}


AppMenuModel::~AppMenuModel() = default;

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
        onWindowChanged(m_currentWindowId);
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
    Q_UNUSED(parent);

    if (!m_menuAvailable || !m_menu) {
        return 0;
    }

    return m_menu->actions().count();
}

void AppMenuModel::update()
{
    beginResetModel();
    endResetModel();
    m_updatePending = false;
}


void AppMenuModel::onActiveWindowChanged(WId id)
{
    qApp->removeNativeEventFilter(this);

    auto pw = qobject_cast<QWidget*>(parent());

    if(pw){
    	if(id == pw->effectiveWinId()) {
		/// Get more options for search
		if(m_initialApplicationFromWindowId == -1) {
			return;
		}
            	
		
		if (KWindowSystem::isPlatformX11()) {
        		auto *c = QX11Info::connection();

        		auto getWindowPropertyString = [c, this](WId id, const QByteArray & name) -> QByteArray {
            		QByteArray value;

            		if (!s_atoms.contains(name)) {
                	   const xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(c, false, name.length(), name.constData());
                	   QScopedPointer<xcb_intern_atom_reply_t, 
				          QScopedPointerPodDeleter> atomReply(xcb_intern_atom_reply(c, atomCookie, nullptr));

                	   if (atomReply.isNull()) {
                    		return value;
                	   }

                  	   s_atoms[name] = atomReply->atom;

                	   if (s_atoms[name] == XCB_ATOM_NONE) {
                    		return value;
                	   }
            		}

            		static const long MAX_PROP_SIZE = 10000;
            		auto propertyCookie = xcb_get_property(c, false, id, s_atoms[name], XCB_ATOM_STRING, 0, MAX_PROP_SIZE);
            		QScopedPointer<xcb_get_property_reply_t, 
				       QScopedPointerPodDeleter> propertyReply(xcb_get_property_reply(c, propertyCookie, nullptr));

            		if (propertyReply.isNull())
            		{
                	   return value;
            		}

            		if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8 && propertyReply->value_len > 0)
            		{
                	   const char *data = (const char *) xcb_get_property_value(propertyReply.data());
                	   int len = propertyReply->value_len;

                	   if (data) {
                    	      value = QByteArray(data, data[len - 1] ? len : len - 1);
                	   }
            		}

            	   	   return value;
        		};
	
			const QString serviceName = QString::fromUtf8(
						getWindowPropertyString(m_initialApplicationFromWindowId,
									s_x11AppMenuServiceNamePropertyName));

            		const QString menuObjectPath = QString::fromUtf8(
						getWindowPropertyString(m_initialApplicationFromWindowId, 
									s_x11AppMenuObjectPathPropertyName));

		
			if(m_serviceName == serviceName &&
		  	    m_menuObjectPath == menuObjectPath) {
			    updateApplicationMenu(m_serviceName, m_menuObjectPath);
			}else {
			    m_initialApplicationFromWindowId = -1;
			}
		}else {
			m_initialApplicationFromWindowId = -1;
		}
		return;
    	}
    }

    if (m_winId != -1  && m_winId != id) {
        //! ignore any other window except the one preferred from plasmoid
        return;
    }

    if (!id) {
        setMenuAvailable(false);
        emit modelNeedsUpdate();
        emit menuParsed();
        return;
    }

    if (KWindowSystem::isPlatformX11()) {
        auto *c = QX11Info::connection();

        auto getWindowPropertyString = [c, this](WId id, const QByteArray & name) -> QByteArray {
            QByteArray value;

            if (!s_atoms.contains(name)) {
                const xcb_intern_atom_cookie_t atomCookie = xcb_intern_atom(c, false, name.length(), name.constData());
                QScopedPointer<xcb_intern_atom_reply_t, QScopedPointerPodDeleter> atomReply(xcb_intern_atom_reply(c, atomCookie, nullptr));

                if (atomReply.isNull()) {
                    return value;
                }

                s_atoms[name] = atomReply->atom;

                if (s_atoms[name] == XCB_ATOM_NONE) {
                    return value;
                }
            }

            static const long MAX_PROP_SIZE = 10000;
            auto propertyCookie = xcb_get_property(c, false, id, s_atoms[name], XCB_ATOM_STRING, 0, MAX_PROP_SIZE);
            QScopedPointer<xcb_get_property_reply_t, QScopedPointerPodDeleter> propertyReply(xcb_get_property_reply(c, propertyCookie, nullptr));

            if (propertyReply.isNull())
            {
                return value;
            }

            if (propertyReply->type == XCB_ATOM_STRING && propertyReply->format == 8 && propertyReply->value_len > 0)
            {
                const char *data = (const char *) xcb_get_property_value(propertyReply.data());
                int len = propertyReply->value_len;

                if (data) {
                    value = QByteArray(data, data[len - 1] ? len : len - 1);
                }
            }

            return value;
        };

        auto updateMenuFromWindowIfHasMenu = [this, &getWindowPropertyString](WId id) {
            const QString serviceName = QString::fromUtf8(getWindowPropertyString(id, s_x11AppMenuServiceNamePropertyName));
            const QString menuObjectPath = QString::fromUtf8(getWindowPropertyString(id, s_x11AppMenuObjectPathPropertyName));

            qDebug() << "probono: WM_CLASS" << QString::fromUtf8(getWindowPropertyString(id, QByteArrayLiteral("WM_CLASS"))); // The filename of the binary that opened this window
            // TODO: Bring Alt-tab like functionality into the menu
            // Check: Does Alt-tab show the application name behind " - " for most applications? No....!
            // But WM_CLASS seems to show the application name. Unfortunately, in lowercase... where does it come from? argv[0] apparently.
            // Is there a way to get a nice name, including proper capitalization?
            // TODO: Make menu with the names of all windows (better: all applications?)
            // TODO: Use KWindowSystem raiseWindow to bring it to the front when the menu item is activated

            // We could have the launch command set an environment variable with the nice application name
            // which we could retrieve here like this on FreeBSD (is there a portable way?):
            // Get _NET_WM_PID
            // procstat -e $_NET_WM_PID

            if (!serviceName.isEmpty() && !menuObjectPath.isEmpty()) {
                m_initialApplicationFromWindowId = id;
		updateApplicationMenu(serviceName, menuObjectPath);
		return true;
            }

            return false;
        };

        KWindowInfo info(id, NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);

        if (info.hasState(NET::SkipTaskbar) ||
                info.windowType(NET::UtilityMask) == NET::Utility)  {

            //! hide when the windows or their transient(s) do not have a menu
            if (filterByActive()) {

                KWindowInfo transientInfo = KWindowInfo(info.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);

                while (transientInfo.win()) {
                    if (transientInfo.win() == m_currentWindowId) {
                        filterWindow(info);
                        return;
                    }

                    transientInfo = KWindowInfo(transientInfo.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);
                }
            }

            if (filterByActive()) {
                setVisible(false);
            }

            // rekols: 切换到桌面时要隐藏menubar
            setMenuAvailable(false);
            emit modelNeedsUpdate();
            emit menuParsed();
            return;
        }

        m_currentWindowId = id;

        if (!filterChildren()) {
            KWindowInfo transientInfo = KWindowInfo(info.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);

            // look at transient windows first
            while (transientInfo.win()) {
                if (updateMenuFromWindowIfHasMenu(transientInfo.win())) {
                    filterWindow(info);
                    return;
                }

                transientInfo = KWindowInfo(transientInfo.transientFor(), NET::WMState | NET::WMWindowType | NET::WMGeometry, NET::WM2TransientFor);
            }
        }

        if (updateMenuFromWindowIfHasMenu(id)) {
            filterWindow(info);
            return;
        }

        // monitor whether an app menu becomes available later
        // this can happen when an app starts, shows its window, and only later announces global menu (e.g. Firefox)
        qApp->installNativeEventFilter(this);
        m_delayedMenuWindowId = id;

        //no menu found, set it to unavailable

        setMenuAvailable(false);
        emit modelNeedsUpdate();
        emit menuParsed();

    }
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
        //! then the window and screen geometries can not be trusted for comparison
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

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();

    if (row < 0 || !m_menuAvailable || !m_menu) {
        return QVariant();
    }

    const auto actions = m_menu->actions();

    if (row >= actions.count()) {
        return QVariant();
    }

    if (role == MenuRole) { // TODO this should be Qt::DisplayRole
        return actions.at(row)->text();
    } else if (role == ActionRole) {
        return QVariant::fromValue((void *) actions.at(row));
    }

    return QVariant();
}

void AppMenuModel::updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath)
{
    if (m_serviceName == serviceName && m_menuObjectPath == menuObjectPath) {
        if (m_importer) {
		QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);
        }

        return;
    }

    m_serviceName = serviceName;
    m_serviceWatcher->setWatchedServices(QStringList({m_serviceName}));

    m_menuObjectPath = menuObjectPath;

    if (m_importer) {
        m_importer->deleteLater();
    }

    m_importer = new KDBusMenuImporter(serviceName, menuObjectPath, this);
    if(serviceName =="org.kde.plasma.gmenu_dbusmenu_proxy") {
        QTimer::singleShot(200,this, [=](){m_importer->updateMenu();});
    } else {
    QMetaObject::invokeMethod(m_importer, "updateMenu", Qt::QueuedConnection);
    }

    m_menu = m_importer->menu();

    connect(m_importer.data(), &DBusMenuImporter::menuUpdated, this, [=](QMenu *menu) {
        for (QAction *a : menu->actions()) {

            if(a->menu()) {
		           m_importer->updateMenu(a->menu());

            }
            // signal dataChanged when the action changes
            connect(a, &QAction::changed, this, [this, a] {
                if (m_menuAvailable && m_menu)
                {
                    const int actionIdx = m_menu->actions().indexOf(a);

                    if (actionIdx > -1) {
                        const QModelIndex modelIdx = index(actionIdx, 0);
                        emit dataChanged(modelIdx, modelIdx);
                    }
                }
            });

            connect(a, &QAction::destroyed, this, &AppMenuModel::modelNeedsUpdate);

        }


    if(m_menu && !m_menu->actions().isEmpty() && m_menu->actions().last() == menu->menuAction()) {
            setMenuAvailable(true);
            emit menuParsed();
          }
    });

    connect(m_importer.data(), &DBusMenuImporter::actionActivationRequested, this, [this](QAction * action) {
        // TODO submenus
	if (!m_menuAvailable || !m_menu) {
            return;
        }

        const auto actions = m_menu->actions();
        auto it = std::find(actions.begin(), actions.end(), action);

        if (it != actions.end()) {
            requestActivateIndex(it - actions.begin());
        }
    });
}

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

            if (serviceNameAtom != XCB_ATOM_NONE && objectPathAtom != XCB_ATOM_NONE) { // shouldn't happen
                if (event->atom == serviceNameAtom || event->atom == objectPathAtom) {
                    // see if we now have a menu
                    onActiveWindowChanged(KWindowSystem::activeWindow());
                }
            }
        }
    }

    return false;
}
