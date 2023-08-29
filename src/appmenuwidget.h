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

#ifndef APPMENUWIDGET_H
#define APPMENUWIDGET_H

#include <QWidget>
#include <QMenuBar>
#include <QToolButton>
#include <QEvent>
#include <QPropertyAnimation>
#include <QCompleter>
#include <QLineEdit>
#include <QMessageBox>
#include <QWidgetAction>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDebug>
#include <QLineEdit>
#include <QVariant>
#include "appmenu/appmenumodel.h"
#include "appmenu/menuimporter.h"
class MenuQCalc;
class SearchLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    SearchLineEdit(QWidget *parent = 0) : QLineEdit(parent) { }
    void keyPressEvent(QKeyEvent *event) override;
};

class CloneAction : public QAction
{
    Q_OBJECT
public:
    CloneAction(QAction *original, QObject *parent = 0) : QAction(parent)
    {
        m_orig = original;
        connect(m_orig, &QAction::changed, this, &CloneAction::updateMe); // update on change
        connect(m_orig, &QAction::destroyed, this, &QAction::deleteLater); // delete on destroyed
        connect(this, &QAction::triggered, m_orig, &QAction::triggered); // trigger on triggered
    }
public slots:
    void updateMe()
    {
        const QStringList props = QStringList() << "autoRepeat"
                                                << "checkable"
                                                << "checked"
                                                << "enabled"
                                                << "icon"
                                                << "iconText"
                                                << "iconVisibleInMenu"
                                                << "statusTip"
                                                << "toolTip"
                                                << "whatsThis";
        foreach (const QString prop, props) {
            setProperty(qPrintable(prop), m_orig->property(qPrintable(prop)));
        }
    }
    void setDisconnectOnClear(QMetaObject::Connection con) { m_con = con; }
    void disconnectOnClear() { QObject::disconnect(m_con); }
    void resetOrigShortcutContext() { m_orig->setShortcutContext(Qt::ApplicationShortcut); }

private:
    QAction *m_orig;
    QMetaObject::Connection m_con;
};

class AppMenuWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AppMenuWidget(QWidget *parent = nullptr);
    ~AppMenuWidget();
    void updateMenu();
    void toggleMaximizeWindow();
    QMenuBar *m_menuBar;

    void focusMenu();
signals:
    void menuAboutToBeImported();

protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event)
            override; // Make it possible to click on the menu entry for a submenu

private:
    MenuQCalc *m_menuQCalc;
    bool m_isSearching = false;
    QStringList watchedLocations;
    bool isAcceptWindow(WId id);
    void delayUpdateActiveWindow();
    void onActiveWindowChanged();
    void onWindowChanged(WId id, NET::Properties properties, NET::Properties2 properties2);
    void minimizeWindow();
    void closeWindow();
    void maxmizeWindow();
    void restoreWindow();

public slots:
    void rebuildMenu();
    void findAppsInside(QStringList locationsContainingApps);
    void actionAbout();
    void actionLaunch(QAction *action);
    void openPath(QAction *action);
    // void actionDisplays();
    // void actionShortcuts();
    // void actionSound();
    void actionMinimizeAll();
    void actionMaximizeAll();
    void actionLogout();
    void actionForceQuit();
    bool which(QString command);

private slots:
    //   void handleActivated(const QString&);
    void searchMenu();
    /// For Action Search
private:
    void updateActionSearch();

    /// For System Main Menu.
private:
    QMenu *m_systemMenu;
    bool m_searchMenuOpened = false;
    QMenu *m_searchMenu;
    QList<QAction *> searchResults;
    QMap<QAction *, QString *> filteredActions;
    void integrateSystemMenu(QMenuBar *);
    void searchEditingDone();
    void refreshTimer();
    void addAppToMenu(QString candidate, QMenu *submenu);

private:
    QFileSystemWatcher *watcher;
    QWidget *searchLineWidget;
    SearchLineEdit *searchLineEdit;
    QCompleter *actionCompleter;
    AppMenuModel *m_appMenuModel;
    MenuImporter *m_menuImporter;
    QWidget *m_buttonsWidget;
    struct cmpAction
    {
        std::string pText;
        std::string text;
        int row;
    };
    friend inline bool operator==(struct AppMenuWidget::cmpAction &lhs,
                                  const struct AppMenuWidget::cmpAction &rhs)
    {
        return lhs.pText == rhs.pText && lhs.text == rhs.text && lhs.row == rhs.row;
    }

    std::vector<cmpAction> m_wasVisible;
    WId m_currentWindowID = 0;
    // QToolButton *m_minButton;
    // QToolButton *m_restoreButton;
    // QToolButton *m_closeButton;
    // QPropertyAnimation *m_buttonsAnimation;
    WId m_windowID;
    QTimer *m_typingTimer;
    QTimer *m_clearTimer;
    // int m_buttonsWidth;

    void keyPressEvent(QKeyEvent *event) override;

    void populateSystemMenu(QWidget *parent);
};

class AboutDialog : public QDialog
{
    QLabel *_imageLabel, *_textLabel;
    QPushButton *_hardwareProbeButton;
    QPushButton *_copyButton;
    QVBoxLayout *_layout;

public:
    AboutDialog(QWidget *parent);
    ~AboutDialog();
};

#endif // APPMENUWIDGET_H
