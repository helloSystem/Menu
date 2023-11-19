#ifndef DATETIME_H
#define DATETIME_H

#include "pluginsinterface.h"
#include "controlwidget.h"
#include <QObject>

class Plugin : public QObject, StatusBarExtension
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.panda.statusbar/1.0")
    Q_INTERFACES(StatusBarExtension)

public:
    explicit Plugin(QObject *parent = nullptr) : QObject(parent), m_widget(new ControlWidget) { }

    QString pluginName() override { return "volume"; }
    QWidget *itemWidget() override { return m_widget; }
    QWidget *popupWindow() override { return m_widget->popupWindow(); }
    QString popupText() override { return m_widget->popupText(); }

private:
    ControlWidget *m_widget;
};

#endif
