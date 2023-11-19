#ifndef DATETIME_H
#define DATETIME_H

#include "pluginsinterface.h"
#include "datetimewidget.h"
#include <QObject>

class DateTime : public QObject, StatusBarExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.panda.statusbar/1.0")
    Q_INTERFACES(StatusBarExtension)

public:
    explicit DateTime(QObject *parent = nullptr);

    QString pluginName() override { return "datetime"; }
    QWidget *itemWidget() override { return new DateTimeWidget; }
};

#endif
