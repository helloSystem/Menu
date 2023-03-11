/*
 * This file is based on QtFM <https://qtfm.eu>
 *
 * Copyright (C) 2010-2021 QtFM developers (see https://github.com/rodlie/qtfm)
 * Copyright (C) 2022 helloSystem developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include "thumbnails.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>

Thumbnail::Thumbnail(const QString &filename,
                     QCryptographicHash::Algorithm hash,
                     int iconSize,
                     QObject *parent)
    : QObject(parent)
    , _filename(filename)
    , _hash(hash)
    , _iconSize(iconSize)
    , _valid(false)
{
}

const QString Thumbnail::getCachePath()
{
    QString result = qgetenv("XDG_CACHE_HOME");
    if (result.isEmpty()) {
        result = QString("%1/.cache").arg(QDir::homePath());
    }
    result.append("/thumbnails");
    switch (_iconSize) {
    case ThumbnailSizeNormal:
        result.append("/normal");
        break;
    case ThumbnailSizeLarge:
        result.append("/large");
        break;
    case ThumbnailSizeXLarge:
        result.append("/x-large");
        break;
    case ThumbnailSizeXXLarge:
        result.append("/xx-large");
        break;
    }
    if (!QFile::exists(result)) {
        QDir dir(result);
        if (!dir.mkpath(result)) { result.clear(); }
    }
    return result;
}

const QString Thumbnail::getIconPath()
{
    QString cache = getCachePath();
    QString hash = getFileHash();
    if (cache.isEmpty() || hash.isEmpty()) { return QString(); }
    return QString("%1/%2.png").arg(cache).arg(hash);
}

bool Thumbnail::isValid()
{
    return _valid;
}

const QString Thumbnail::getFileHash()
{
    if (!QFile::exists(_filename)) { return QString(); }
    return QString(QCryptographicHash::hash(QUrl::fromUserInput(_filename).toString().toUtf8(), _hash).toHex());
}

Thumbnails::Thumbnails(QObject *parent)
    : QObject(parent)
    , _busy(false)
    , _enabled(true)
{
}

bool Thumbnails::isBusy()
{
    return _busy;
}

void Thumbnails::setEnabled(bool enabled)
{
    _enabled = enabled;
}

bool Thumbnails::isEnabled()
{
    return _enabled;
}

void Thumbnails::requestIcon(const QString &filename,
                             QCryptographicHash::Algorithm hash,
                             int iconSize)
{
    qDebug() << "requestIcon" << filename << hash << iconSize;
    QtConcurrent::run(this,
                      &Thumbnails::getIcon,
                      filename,
                      hash,
                      iconSize);
}

void Thumbnails::requestIcons(const QStringList &filenames,
                              QCryptographicHash::Algorithm hash,
                              int iconSize)
{
    for (int i = 0; i < filenames.size(); ++i) {
        requestIcon(filenames.at(i), hash, iconSize);
    }
}

void Thumbnails::getIcon(const QString &filename,
                         QCryptographicHash::Algorithm hash,
                         int iconSize)
{
    if (!isEnabled()) { return; }
    _busy = true;
    qDebug() << "getIcon" << filename << hash << iconSize;
    Thumbnail thumb(filename, hash, iconSize);
    _busy = false;
    if (thumb.isValid()) { emit foundIcon(filename, thumb.getIconPath()); }
}
