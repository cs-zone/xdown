#pragma once

#include <QSet>
#include <QString>
#include <QVector>
#include <qmap.h>

#include "../tristatebool.h"

namespace BitTorrent
{
    struct AddXDownParams
    {
        QString name;
        QString savePath;
        int downloadLimit = -1;

        QString url;

        QMap<QString, QString> reqHeader;

        QMap<QString, QString> uriOption;
    };
}
