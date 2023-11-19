#include "datetimewidget.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QLocale>
#include <QProcess>
#include <QDebug>

DateTimeWidget::DateTimeWidget(QWidget *parent)
    : QWidget(parent),
      m_menubar(new QMenuBar),
      m_menu(new QMenu),
      dateAction(new QAction),
      m_refreshTimer(new QTimer(this))
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setAlignment(Qt::AlignCenter); // Center QHBoxLayout vertically
    layout->setMargin(0);
    layout->setSpacing(0);
    // m_label->setStyleSheet("padding-top: 2px;"); // FIXME: Find a way to achieve vertically
    // centered text without this crude workaround m_label->setStyleSheet("color: grey;"); // Does
    // not work; presumably due the QtPlugin theme overriding it

    layout->addWidget(m_menubar);
    m_menubar->addMenu(m_menu);
    setLayout(layout);

    // Add menu items
    m_menu->addAction(dateAction);
    connect(dateAction, &QAction::triggered, this, &DateTimeWidget::change);

    updateCurrentTimeString();

    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();
    connect(m_refreshTimer, &QTimer::timeout, this, &DateTimeWidget::updateCurrentTimeString);
}

void DateTimeWidget::updateCurrentTimeString()
{
    m_menu->menuAction()->setText(
            QLocale::system().toString(QTime::currentTime(), QLocale::ShortFormat)); // Only time
    // m_menu->menuAction()->setText(QLocale::system().toString(QDateTime::currentDateTime(),
    // QLocale::ShortFormat)); // Date and time
    dateAction->setText(QLocale::system().toString(
            QDateTime::currentDateTime(),
            QLocale::LongFormat)); // Weekday, date and time with seconds and timezone
}

void DateTimeWidget::change()
{
    qDebug() << "probono: To be implemented";
    QProcess *p = new QProcess();
    p->setArguments({ "Date and Time" });
    p->setProgram("launch");
    p->startDetached();
}
