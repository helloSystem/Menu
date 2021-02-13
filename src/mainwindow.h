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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFrame>
#include "mainpanel.h"

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    MainPanel *m_mainPanel;
    ~MainWindow();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void initSize();
    void setStrutPartial();

private:
    QWidget *m_fakeWidget;
};

#endif // MAINWINDOW_H
