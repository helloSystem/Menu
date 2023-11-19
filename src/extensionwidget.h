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

#ifndef EXTENSIONWIDGET_H
#define EXTENSIONWIDGET_H

#include <QWidget>
#include <QTimer>
#include "blurwindow.h"
#include "popupwindow.h"
#include "../interfaces/pluginsinterface.h"

class ExtensionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ExtensionWidget(StatusBarExtension *extension = nullptr, QWidget *parent = nullptr);

    QSize sizeHint() const override;

protected:
    void enterEvent(QEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QPoint popupPoint(int contentWidth);
    void showPopupText();
    void hidePopupText();
    void showPopupWindow();

private:
    StatusBarExtension *m_extension;
    QTimer *m_popupTextDelayTimer;
    BlurWindow *m_popupText;
    PopupWindow *m_popupWindow;
    QWidget *m_contentWidget;
};

#endif // EXTENSIONWIDGET_H
