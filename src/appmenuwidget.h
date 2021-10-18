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

#ifndef APPMENUWIDGET_H
#define APPMENUWIDGET_H

#include <QWidget>
#include <QMenuBar>
#include <QToolButton>
#include <QPropertyAnimation>
#include <QCompleter>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileSystemWatcher>

#include "appmenu/appmenumodel.h"
#include "appmenu/menuimporter.h"

#include "actionsearch/actionsearch.h"

class AppMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppMenuWidget(QWidget *parent = nullptr);
    ~AppMenuWidget();

    void updateMenu();
    void toggleMaximizeWindow();
    QMenuBar *m_menuBar;
    QFileSystemWatcher *watcher;

protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override; // Make it possible to click on the menu entry for a submenu

private:
    bool isAcceptWindow(WId id);
    void delayUpdateActiveWindow();
    void onActiveWindowChanged();
    void onWindowChanged(WId id, NET::Properties properties, NET::Properties2 properties2);
    void minimizeWindow();
    void closeWindow();
    void maxmizeWindow();
    void restoreWindow();

public slots:
    void rebuildMenu();
    void findAppsInside(QStringList locationsContainingApps, QMenu *m_systemMenu,  QFileSystemWatcher *watcher);
    void actionAbout();
    void actionLaunch(QAction *action);
    // void actionDisplays();
    // void actionShortcuts();
    // void actionSound();
    void actionMinimizeAll();
    void actionMaximizeAll();
    void actionLogout();
    bool which(QString command);

private slots:
    void handleActivated(const QString&);

/// For Action Search
private:
    void updateActionSearch(QMenuBar*);

/// For System Main Menu.
private:
    QMenu *m_systemMenu;
    void integrateSystemMenu(QMenuBar*);

private:
    QWidget *searchLineWidget;
    QLineEdit *searchLineEdit;
    QCompleter *actionCompleter;
    ActionSearch *actionSearch;
    AppMenuModel *m_appMenuModel;
    MenuImporter *m_menuImporter;
    QWidget *m_buttonsWidget;
    // QToolButton *m_minButton;
    // QToolButton *m_restoreButton;
    // QToolButton *m_closeButton;
    //QPropertyAnimation *m_buttonsAnimation;
    WId m_windowID;
    
    //int m_buttonsWidth;
};

#endif // APPMENUWIDGET_H
