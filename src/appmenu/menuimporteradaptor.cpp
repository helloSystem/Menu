/*
 * This file was generated by qdbusxml2cpp version 0.8
 * Command line was: qdbusxml2cpp -m -a menuimporteradaptor -c MenuImporterAdaptor -i menuimporter.h
 * -l MenuImporter
 * /home/septemberhx/Workspace/git/dde-top-panel/appmenu/com.canonical.AppMenu.Registrar.xml
 *
 * qdbusxml2cpp is Copyright (C) 2017 The Qt Company Ltd.
 *
 * This is an auto-generated file.
 * Do not edit! All changes made to it will be lost.
 */

#include "menuimporteradaptor.h"
#include <QtCore/QMetaObject>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>

/*
 * Implementation of adaptor class MenuImporterAdaptor
 */

MenuImporterAdaptor::MenuImporterAdaptor(MenuImporter *parent) : QDBusAbstractAdaptor(parent)
{
    // constructor
    setAutoRelaySignals(true);
}

MenuImporterAdaptor::~MenuImporterAdaptor()
{
    // destructor
}

QString MenuImporterAdaptor::GetMenuForWindow(uint windowId, QDBusObjectPath &menuObjectPath)
{
    // handle method call com.canonical.AppMenu.Registrar.GetMenuForWindow
    return parent()->GetMenuForWindow(windowId, menuObjectPath);
}

void MenuImporterAdaptor::RegisterWindow(uint windowId, const QDBusObjectPath &menuObjectPath)
{
    // handle method call com.canonical.AppMenu.Registrar.RegisterWindow
    parent()->RegisterWindow(windowId, menuObjectPath);
}

void MenuImporterAdaptor::UnregisterWindow(uint windowId)
{
    // handle method call com.canonical.AppMenu.Registrar.UnregisterWindow
    parent()->UnregisterWindow(windowId);
}
