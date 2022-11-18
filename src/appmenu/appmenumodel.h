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
class HMenu :public QMenu {
public:
    HMenu(QWidget* parent=0):QMenu(parent) {


    }
protected:
    void actionEvent(QActionEvent *e)  {

        if(e->type() == QEvent::ActionAdded) {
            if (qobject_cast<QMenuBar*>(parent())!=nullptr) {
                if(e->action()->menu())
                    qobject_cast<QMenuBar*>(parent())->addMenu(e->action()->menu());

           }


        }
       if(e->type() == QEvent::ActionRemoved) {
            if (qobject_cast<QMenuBar*>(parent())!=nullptr) {
                     qobject_cast<QMenuBar*>(parent())->removeAction(e->action());

        }
       }

}
};


class HDBusMenuImporter : public DBusMenuImporter
{
public:
    HDBusMenuImporter(const QString &service, const QString &path, const enum DBusMenuImporterType type, QObject *parent=0)
    : DBusMenuImporter(service, path, type,parent)

    {

    }

    QMenu *createMenu(QWidget *parent) override {
        HMenu * menu = new HMenu(parent);
        return menu;
    }

};
class AppMenuModel : public QAbstractItemModel, public QAbstractNativeEventFilter
{
    Q_OBJECT

    Q_PROPERTY(bool menuAvailable READ menuAvailable WRITE setMenuAvailable NOTIFY menuAvailableChanged)
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)

    Q_PROPERTY(bool filterByActive READ filterByActive WRITE setFilterByActive NOTIFY filterByActiveChanged)
    Q_PROPERTY(bool filterChildren READ filterChildren WRITE setFilterChildren NOTIFY filterChildrenChanged)

    Q_PROPERTY(QRect screenGeometry READ screenGeometry WRITE setScreenGeometry NOTIFY screenGeometryChanged)

    Q_PROPERTY(QVariant winId READ winId WRITE setWinId NOTIFY winIdChanged)
public:
    explicit AppMenuModel(QObject *parent = nullptr);
    ~AppMenuModel() override;

    enum AppMenuRole
    {
        MenuRole = Qt::UserRole + 1, // TODO this should be Qt::DisplayRole
        ActionRole
    };
    QAction *findParent(QAction * child,QAction *root) const ;
    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    QMap<QString,QAction*> filteredActions() { return m_visibleActions; }
    void updateApplicationMenu(const QString &serviceName, const QString &menuObjectPath);
    void updateSearch();
    void invalidateMenu() { m_menu=nullptr;}
    bool filterByActive() const;
    void setFilterByActive(bool active);
    // void execute(QString actionName);
    bool filterChildren() const;
    void setFilterChildren(bool hideChildren);
    bool menuAvailable() const;
    void setMenuAvailable(bool set);
    void clearFilteredActions() { m_visibleActions.clear();}
    bool filterMenu(QMenu *searchMenu, QString searchString, bool includeDisabled, QStringList names);
    bool visible() const;
    QList<QAction*>   mActions() { return m_actions; }
    QRect screenGeometry() const;
    void setScreenGeometry(QRect geometry);

    QVariant winId() const;
    void setWinId(const QVariant &id);
    void readMenuActions(QMenu* menu,QStringList names);
    QPointer<QMenu> menu() { return m_menu; }
    void refreshSearch();
signals:
    void requestActivateIndex(int index);


protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, long int *result) override;
public Q_SLOTS:


private Q_SLOTS:

    void onActiveWindowChanged(WId id);
    void onWindowChanged(WId id);
    //! there are apps that are not releasing their menu properly after closing
    //! and as such their menu is still shown even though the app does not exist
    //! any more. Such apps are Java based e.g. smartgit
    void onWindowRemoved(WId id);
    void filterWindow(KWindowInfo &info);

    void setVisible(bool visible);
    void update();


signals:
    void menuAvailableChanged();
    void modelNeedsUpdate();
    void menuImported();
    void firstLevelParsed();
    void filterByActiveChanged();
    void filterChildrenChanged();
    void visibleChanged();
    void screenGeometryChanged();
    void winIdChanged();

private:
    void deleteActions(QMenu * menu());
    QWidget *w_parent;
    QMap<QString,QAction*> m_names;
    bool m_filterByActive = false;
    bool m_filterChildren = false;
    bool m_menuAvailable;
    bool m_updatePending = false;
    bool m_visible = true;
    bool m_awaitsUpdate;
    QRect m_screenGeometry;
    bool    hasVisible =false;
    QMap<QString,QAction*> m_visibleActions;
    QVariant m_winId{-1};
    //! current active window used
    WId m_currentWindowId = 0;
    WId m_initialApplicationFromWindowId = -1;
    //! window that its menu initialization may be delayed
    WId m_delayedMenuWindowId = 0;
    QPointer<QMenu>  m_menu;
    QList<QAction*> m_actions;
    QDBusServiceWatcher *m_serviceWatcher;
    QString m_serviceName;
    QString m_menuObjectPath;
    bool m_refreshSearch;
    DBusMenuImporter *m_importer;
    QHash<QString,DBusMenuImporter*> m_importers;
};

#endif
