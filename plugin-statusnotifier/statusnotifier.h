#ifndef STATUSNOTIFIER_H
#define STATUSNOTIFIER_H

#include "pluginsinterface.h"
#include "statusnotifierwidget.h"
#include <QObject>

class Statusnotifier : public QObject, StatusBarExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.panda.statusbar/1.0")
    Q_INTERFACES(StatusBarExtension)

public:
    explicit Statusnotifier(QObject *parent = nullptr);

    QString pluginName() override { return "statusnotifier"; }
    QWidget *itemWidget() override { return new StatusNotifierWidget; }
};

#endif
