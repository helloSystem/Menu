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

#ifndef MAINPANEL_H
#define MAINPANEL_H

#include <QWidget>
#include <QHBoxLayout>
#include "../interfaces/pluginsiterface.h"
#include "pluginmanager.h"
#include "appmenuwidget.h"

class MainPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MainPanel(QWidget *parent = nullptr);

    void loadModules();
    void loadModule(const QString &pluginName, QHBoxLayout *layout);
    void rebuildSystemMenu();

protected:
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    QHBoxLayout *m_globalMenuLayout;
    QHBoxLayout *m_statusnotifierLayout;
    QHBoxLayout *m_controlCenterLayout;
    QHBoxLayout *m_dateTimeLayout;
    AppMenuWidget *m_appMenuWidget;

    PluginManager *m_pluginManager;
};

#endif // MAINPANEL_H
