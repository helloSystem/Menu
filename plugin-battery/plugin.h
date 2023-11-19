#ifndef PLUGIN_H
#define PLUGIN_H

#include "pluginsinterface.h"
#include "batterywidget.h"
#include <QObject>

class Plugin : public QObject, StatusBarExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.panda.statusbar/1.0")
    Q_INTERFACES(StatusBarExtension)

public:
    explicit Plugin(QObject *parent = nullptr) : QObject(parent), m_widget(new BatteryWidget) { }

    QString pluginName() override { return "battery"; }
    QWidget *itemWidget() override { return m_widget; }
    QString popupText() override { return m_widget->popupText(); }
    /* QWidget *popupWindow() override { return m_widget->popupWindow(); } */

private:
    BatteryWidget *m_widget;
};

#endif
