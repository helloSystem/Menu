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

#include "mainpanel.h"
#include "extensionwidget.h"

#include <QMouseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QStyle>
#include <QApplication>
#include <QProcess>
#include <QFileInfo>
#include <QPainter>
#include <QMessageBox>
#include <QDialog>

//store our layout for rebuilding the menu
QHBoxLayout* m_layout;

MainPanel::MainPanel(QWidget *parent)
    : QWidget(parent),
      m_globalMenuLayout(new QHBoxLayout),
      m_statusnotifierLayout(new QHBoxLayout),
      m_controlCenterLayout(new QHBoxLayout),
      m_dateTimeLayout(new QHBoxLayout),
      m_appMenuWidget(new AppMenuWidget),
      m_pluginManager(new PluginManager(this))
{
    m_pluginManager->start();

    m_appMenuWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QWidget *statusnotifierWidget = new QWidget;
    statusnotifierWidget->setLayout(m_statusnotifierLayout);
    // statusnotifierWidget->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    statusnotifierWidget->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum)); // Naming is counterintuitive. "Maximum" keeps its size to a minimum!
    m_statusnotifierLayout->setMargin(0);

    QWidget *dateTimeWidget = new QWidget;
    dateTimeWidget->setLayout(m_dateTimeLayout);
    // dateTimeWidget->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    dateTimeWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding); // Naming is counterintuitive. "Maximum" keeps its size to a minimum! Need "Expanding" in y direction so that font will be centered
    m_dateTimeLayout->setMargin(0);

    m_controlCenterLayout->setSpacing(10);

    QHBoxLayout *layout = new QHBoxLayout;
    layout->setSpacing(0);
    // layout->addSpacing(10);
    layout->addWidget(m_appMenuWidget); // Main menu including Action Search
    // layout->addStretch();
    layout->addSpacing(10);
    layout->addLayout(m_controlCenterLayout); // Volume, etc.
    layout->addSpacing(10);
    layout->addWidget(statusnotifierWidget); // Tray applications. TODO: More before m_controlCenterLayout once m_controlCenterLayout is looking good and us functional
    layout->addSpacing(10);
    layout->addWidget(dateTimeWidget);
    layout->addSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    loadModules();
    m_layout = layout;
}

void MainPanel::rebuildSystemMenu()
{
    qDebug() << "SIGSUR1 recived, rebuild the system menu";

    if(m_appMenuWidget){
        m_layout->removeWidget(m_appMenuWidget);
        delete m_appMenuWidget;
        m_appMenuWidget = new AppMenuWidget();
        m_appMenuWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_layout->insertWidget(0, m_appMenuWidget);
    }
}

void MainPanel::loadModules()
{
    loadModule("datetime", m_dateTimeLayout);
    loadModule("statusnotifier", m_statusnotifierLayout);
    loadModule("volume", m_controlCenterLayout);
    loadModule("battery", m_controlCenterLayout);
}

void MainPanel::loadModule(const QString &pluginName, QHBoxLayout *layout)
{
    ExtensionWidget *extensionWidget = m_pluginManager->plugin(pluginName);
    if (extensionWidget) {
        // extensionWidget->setFixedHeight(rect().height());
        layout->addWidget(extensionWidget);
    }
}

void MainPanel::mouseDoubleClickEvent(QMouseEvent *e)
{
    QWidget::mouseDoubleClickEvent(e);

    // if (e->button() == Qt::LeftButton)
    //     m_appMenuWidget->toggleMaximizeWindow();
}

