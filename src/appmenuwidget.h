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
#include "appmenu/appmenumodel.h"
#include "appmenu/menuimporter.h"

#include "actionsearch/actionsearch.h"
#include "actionsearch/ui/dialog.h"

class AppMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppMenuWidget(QWidget *parent = nullptr);

    void updateMenu();
    void toggleMaximizeWindow();
    QMenuBar *m_menuBar;

protected:
    bool event(QEvent *e) override;

private:
    bool isAcceptWindow(WId id);
    void delayUpdateActiveWindow();
    void onActiveWindowChanged();
    void onWindowChanged(WId id, NET::Properties properties, NET::Properties2 properties2);
    void minimizeWindow();
    void clsoeWindow();
    void maxmizeWindow();
    void restoreWindow();

public slots:
    void actionAbout();
    void actionDisplays();
    void actionShortcuts();
    void actionSound();
    void actionLogout();
    bool which(QString command);

private slots:
    void acceptActionDialog();

private:
    ActionSearch *actionSearch;
    Dialog *actionDialog;
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
