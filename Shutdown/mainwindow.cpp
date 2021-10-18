#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QProcess>
#include <QStyle>
#include <QDesktopWidget>
#include <QTime>
#include <QPropertyAnimation>
#include <QIcon>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setGeometry(
                QStyle::alignedRect(
                    Qt::LeftToRight,
                    Qt::AlignCenter,
                    this->size(),
                    qApp->desktop()->availableGeometry()
                    )
                );
    ui->iconLabel->setPixmap(QIcon::fromTheme("exit").pixmap(48));
    // TODO: Find a way to make this grayscale rather than color
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_logoutButton_clicked()
{
    // this->clearScreen();
    QProcess::execute("killall", QStringList() << "sh");
}

void MainWindow::on_restartButton_clicked()
{
    // this->clearScreen();
    QProcess::execute("sudo", QStringList() << "shutdown" << "-r" << "now");
}

void MainWindow::on_shutdownButton_clicked()
{
    // this->clearScreen();
    QProcess::execute("sudo", QStringList() << "shutdown" << "-p" << "now");
}

// When Xorg gets killed, window decorations disappear first, the whole thing is unsightly
// hence we fill the screen before we exit
void MainWindow::clearScreen()
{

    // TODO: Is there a way to tell all applications that they should save now?

    /*

    QWidget w;
    QPalette p = w.palette();
    p.setColor(QPalette::Window, Qt::gray);
    w.setPalette(p);
    w.setAutoFillBackground(true);

    // TODO: Animate

    w.showFullScreen();

    // Wait one second so that we can see something
    // TODO: Shorten or remove if not needed

    QTime dieTime= QTime::currentTime().addSecs(1);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);

    // FIXME: Make sure we get killed as the very last process in Xorg

    */

}
