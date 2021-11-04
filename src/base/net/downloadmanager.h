/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2015, 2018  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QMutex>

class QNetworkCookie;
class QNetworkReply;
class QSslError;
class QUrl;

namespace Net
{
    struct ServiceID
    {
        QString hostName;
        int port;

        static ServiceID fromURL(const QUrl &url);
    };

    uint qHash(const ServiceID &serviceID, uint seed);
    bool operator==(const ServiceID &lhs, const ServiceID &rhs);

    enum class DownloadStatus
    {
        Success,
        RedirectedToMagnet,
        Failed
    };

    class DownloadRequest
    {
    public:
        DownloadRequest(const QString &url);
        DownloadRequest(const DownloadRequest &other) = default;

        QString url() const;
        DownloadRequest &url(const QString &value);

        QString userAgent() const;
        DownloadRequest &userAgent(const QString &value);

        QHash<QString, QString> getUserHeaders() const;
        Net::DownloadRequest &setUserHeaders(const QHash<QString, QString> &value);

        qint64 limit() const;
        DownloadRequest &limit(qint64 value);

        bool saveToFile() const;
        DownloadRequest &saveToFile(bool value);

        QString getXSNI() const { return x_sni; };
        void setXSNI(const QString& strVal) { x_sni = strVal; };

        QString getXHost() const { return x_host; };
        void setXHost(const QString& strVal) { x_host = strVal; };

        QString getXIpAddr() const { return x_ipAddr; };
        void setXIpAddr(const QString& strVal) { x_ipAddr = strVal; };

        int getXPort() const { return x_port; };
        void setXPort(const int& iVal) { x_port = iVal; };

        bool getUseDefaultRef() const;
        void setUseDefaultRef(bool bValue);
        bool getUseDefaultGZip() const;
        void setUseDefaultGZip(bool bValue);

    private:
        QString m_url;
        QString m_userAgent;
        QHash<QString, QString> m_userHeaders;
        qint64 m_limit = 0;
        bool m_saveToFile = false;
        bool m_useDefaultRef = true;
        bool m_useDefaultGZip = true;

        QString  x_sni;
        QString  x_host;
        QString  x_ipAddr;
        int      x_port = 0;
    };

    struct DownloadResult
    {
        QString url;
        DownloadStatus status;
        QString errorString;
        QByteArray data;
        QString filePath;
        QString magnet;
    };

    class DownloadHandler : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(DownloadHandler)

    public:
        using QObject::QObject;

        virtual void cancel() = 0;

    signals:
        void finished(const DownloadResult &result);
    };

    class DownloadManager : public QObject
    {
        Q_OBJECT
        Q_DISABLE_COPY(DownloadManager)

    public:
        static void initInstance();
        static void freeInstance();
        static DownloadManager *instance();

        DownloadHandler *download(const DownloadRequest &downloadRequest);

        template <typename Context, typename Func>
        void download(const DownloadRequest &downloadRequest, Context context, Func &&slot);

        void registerSequentialService(const ServiceID &serviceID);

        QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const;
        bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);
        QList<QNetworkCookie> allCookies() const;
        void setAllCookies(const QList<QNetworkCookie> &cookieList);
        bool deleteCookie(const QNetworkCookie &cookie);

        static bool hasSupportedScheme(const QString &url);

    private slots:
        void ignoreSslErrors(QNetworkReply *, const QList<QSslError> &);

    private:
        explicit DownloadManager(QObject *parent = nullptr);

        void applyProxySettings();
        void handleReplyFinished(const QNetworkReply *reply);

        static DownloadManager *m_instance;
        QNetworkAccessManager m_networkManager;

        QSet<ServiceID> m_sequentialServices;
        QSet<ServiceID> m_busyServices;
        QMutex                                  m_waitingMutex;
        QHash<ServiceID, QQueue<DownloadHandler *>> m_waitingJobs;
    };

    template <typename Context, typename Func>
    void DownloadManager::download(const DownloadRequest &downloadRequest, Context context, Func &&slot)
    {
        const DownloadHandler *handler = download(downloadRequest);
        connect(handler, &DownloadHandler::finished, context, slot);
    }
}
