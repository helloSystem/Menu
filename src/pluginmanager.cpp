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

#include "pluginmanager.h"
#include <QDir>
#include <QPluginLoader>
#include <QDebug>
#include <QApplication>
#include <QList>

PluginManager::PluginManager(QObject *parent)
  : QObject(parent)
{

}

void PluginManager::start()
{
    QList<QDir> pluginsDirs;

    // Load plugins from FHS location
    auto pluginsDir = QDir(QDir(QCoreApplication::applicationDirPath() + \
        QString("/Resources/plugins")).canonicalPath());
    pluginsDirs.append(pluginsDir);

    // Load plugins from build/ subdirectories (useful during development)
    QStringList filters;
    filters << "plugin-*";
    const QStringList dirs = QDir(QCoreApplication::applicationDirPath() + \
                            QString("/../")).entryList(filters);

    {
        for(const QString &dir : dirs) {
            auto additionalPluginsDir = QDir(QCoreApplication::applicationDirPath() + \
                                             QString("/../") + dir);
            pluginsDirs.append(QDir(additionalPluginsDir.canonicalPath()));
        }
    }

    for(const auto &pluginsDir : pluginsDirs) {
        const QFileInfoList files = pluginsDir.entryInfoList(QDir::Files);
        for (const QFileInfo &file : files) {
            const QString filePath = file.filePath();
            if (!QLibrary::isLibrary(filePath))
                continue;

            QPluginLoader *loader = new QPluginLoader(filePath);
            StatusBarExtension *plugin = qobject_cast<StatusBarExtension *>(loader->instance());

            if (plugin) {
                qDebug() << "loaded " << plugin->pluginName() << " !!!";
                ExtensionWidget *widget = new ExtensionWidget(plugin);
                m_extensions.insert(plugin->pluginName(), widget);
            } else {
                qDebug() << filePath << loader->errorString();
            }
        }
    }
}

ExtensionWidget* PluginManager::plugin(const QString &pluginName)
{
    return m_extensions.value(pluginName, nullptr);
}
