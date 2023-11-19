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

#ifndef MainWidget_H
#define MainWidget_H

#include <QWidget>
#include <QHBoxLayout>
#include "../interfaces/pluginsinterface.h"
#include "pluginmanager.h"
#include "appmenuwidget.h"

class MainWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MainWidget(QWidget *parent = nullptr);

    void loadModules();
    void loadModule(const QString &pluginName, QHBoxLayout *layout);
    void rebuildSystemMenu();
    void triggerFocusMenu();
    AppMenuWidget *getAppMenuWidget() { return m_appMenuWidget; }
signals:
    void menuAboutToBeImported();

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    QHBoxLayout *m_globalMenuLayout;
    QHBoxLayout *m_statusnotifierLayout;
    QHBoxLayout *m_controlCenterLayout;
    QHBoxLayout *m_dateTimeLayout;
    QHBoxLayout *m_windowsLayout;
    AppMenuWidget *m_appMenuWidget;

    PluginManager *m_pluginManager;
};

#endif // MainWidget_H
