/* BEGIN_COMMON_COPYRIGHT_HEADER
 * (c)LGPL2+
 *
 * Authors:
 *  Balázs Béla <balazsbela[at]gmail.com>
 *  Paulo Lieuthier <paulolieuthier@gmail.com>
 *
 * This program or library is free software; you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 *
 * END_COMMON_COPYRIGHT_HEADER */

#ifndef STATUSNOTIFIERBUTTON_H
#define STATUSNOTIFIERBUTTON_H

#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusInterface>
#include <QMouseEvent>
#include <QToolButton>
#include <QWheelEvent>
#include <QMenu>

#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
template<typename T>
inline T qFromUnaligned(const uchar *src)
{
    T dest;
    const size_t size = sizeof(T);
    memcpy(&dest, src, size);
    return dest;
}
#endif

class SniAsync;

class StatusNotifierButton : public QToolButton
{
    Q_OBJECT

public:
    StatusNotifierButton(QString service, QString objectPath, QWidget *parent = nullptr);
    ~StatusNotifierButton();

    enum Status { Passive, Active, NeedsAttention };

    QImage convertToGrayScale(const QImage &srcImage);
public slots:
    void newIcon();
    void newAttentionIcon();
    void newOverlayIcon();
    void newToolTip();
    void newStatus(QString status);

private:
    SniAsync *interface;
    QMenu *m_menu;
    Status m_status;

    QIcon m_icon, m_overlayIcon, m_attentionIcon, m_fallbackIcon;

protected:
    void contextMenuEvent(QContextMenuEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void wheelEvent(QWheelEvent *event);

    void refetchIcon(Status status, const QString &themePath);
    void resetIcon();
};

#endif // STATUSNOTIFIERBUTTON_H
