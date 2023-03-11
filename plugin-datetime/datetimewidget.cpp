#include "datetimewidget.h"
#include <QHBoxLayout>
#include <QDateTime>
#include <QLocale>

DateTimeWidget::DateTimeWidget(QWidget *parent)
    : QWidget(parent),
      m_label(new QLabel),
      m_refreshTimer(new QTimer(this))
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    // m_label->setStyleSheet("padding-top: 2px;"); // FIXME: Find a way to achieve vertically centered text without this crude workaround
    // m_label->setStyleSheet("color: grey;"); // Does not work; presumably due the QtPlugin theme overriding it

    layout->addWidget(m_label);
    setLayout(layout);

    updateCurrentTimeString();

    m_refreshTimer->setInterval(1000);
    m_refreshTimer->start();
    connect(m_refreshTimer, &QTimer::timeout, this, &DateTimeWidget::updateCurrentTimeString);
}

void DateTimeWidget::updateCurrentTimeString()
{
    m_label->setText(QLocale::system().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
}
