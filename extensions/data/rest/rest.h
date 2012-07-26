/*******************************************************************************
* This file is part of PlexyDesk.
*  Maintained by : Siraj Razick <siraj@kde.org>
*  Authored By  :
*
*  PlexyDesk is free software: you can redistribute it and/or modify
*  it under the terms of the GNU Lesser General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  PlexyDesk is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Lesser General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with PlexyDesk. If not, see <http://www.gnu.org/licenses/lgpl.html>
*******************************************************************************/
#ifndef REST_DATA_H
#define REST_DATA_H

#include <QtCore>
#include <plexy.h>
#include <abstractplugininterface.h>
#include <datasource.h>
#include <QtNetwork>

class RestData : public PlexyDesk::DataSource
{
    Q_OBJECT

public:
    RestData(QObject *object = 0);
    virtual ~RestData();
    void init();
    QVariantMap readAll();

public Q_SLOTS:
    void setArguments(QVariant &data);
    void replyFinished(QNetworkReply *reply);
    void handleAuth(QNetworkReply *, QAuthenticator *);

private:
    class Private;
    Private *const d;
};

#endif
