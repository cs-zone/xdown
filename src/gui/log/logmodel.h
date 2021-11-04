/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
 * Copyright (C) 2019  sledgehammer999 <hammered999@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#pragma once

#include <boost/circular_buffer.hpp>

#include <QAbstractListModel>
#include <QColor>
#include <QHash>

#include "base/logger.h"

class BaseLogModel : public QAbstractListModel
{
    Q_DISABLE_COPY(BaseLogModel)

public:
    enum MessageTypeRole
    {
        TimeRole = Qt::UserRole,
        MessageRole,
        TimeForegroundRole,
        MessageForegroundRole,
        TypeRole
    };

    explicit BaseLogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    void reset();

protected:
    class Message
    {
    public:
        Message(const QString &time, const QString &message, const QColor &foreground, Log::MsgType type);

        QVariant time() const;
        QVariant message() const;
        QVariant foreground() const;
        QVariant type() const;

    private:
        QVariant m_time;
        QVariant m_message;
        QVariant m_foreground;
        QVariant m_type;
    };

    void addNewMessage(const Message &message);

private:
    boost::circular_buffer_space_optimized<Message> m_messages;
    const QColor m_timeForeground;
};

class LogMessageModel : public BaseLogModel
{
    Q_OBJECT
    Q_DISABLE_COPY(LogMessageModel)

public:
    explicit LogMessageModel(QObject *parent = nullptr);

private slots:
    void handleNewMessage(const Log::Msg &message);

private:
    const QHash<int, QColor> m_foregroundForMessageTypes;
};

class LogPeerModel : public BaseLogModel
{
    Q_OBJECT
    Q_DISABLE_COPY(LogPeerModel)

public:
    explicit LogPeerModel(QObject *parent = nullptr);

private slots:
    void handleNewMessage(const Log::Peer &peer);

private:
    const QColor m_bannedPeerForeground;
};
