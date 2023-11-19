#ifndef WINDOWS_H
#define WINDOWS_H

#include "pluginsinterface.h"
#include "windowswidget.h"
#include <QObject>

class Windows : public QObject, StatusBarExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.panda.statusbar/1.0")
    Q_INTERFACES(StatusBarExtension)

public:
    explicit Windows(QObject *parent = nullptr);

    QString pluginName() override { return "windows"; }
    QWidget *itemWidget() override { return new WindowsWidget; }
};

#endif
