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

#include <QDir>
#include <QPluginLoader>
#include <QSettings>
#include <QStringList>
#include <QDebug>

#include "pluginloader.h"
#include "extensionfactory.h"

namespace PlexyDesk
{
PluginLoader *PluginLoader::mInstance = 0;

class PluginLoader::Private
{
public:
    Private() {
    }
    ~Private() {
    }
    Interface mPluginGroups;
    QString mPluginPrefix;
    QHash<QString, QStringList> mDict;
};

PluginLoader::PluginLoader() : d(new Private)
{
}

PluginLoader::~PluginLoader()
{
    delete d;
}

PluginLoader *PluginLoader::getInstance()
{
    if (!mInstance) {
        mInstance = new PluginLoader();
        mInstance->scanForPlugins();
    }
    return mInstance;
}

QStringList PluginLoader::listPlugins(const QString &types)
{
    return d->mDict[types];
}

AbstractSource *PluginLoader::instance(const QString &name)
{
    if (d->mPluginGroups.contains(name)) {
        return d->mPluginGroups[name]->instance();
    }  else {
        return 0;
    }
}

void PluginLoader::load(const QString &interface, const QString &pluginName)
{
    // plugin already loaded
    if(d->mPluginGroups.find(pluginName) != d->mPluginGroups.end())
        return;

#ifdef Q_WS_MAC
    QPluginLoader loader (d->mPluginPrefix + pluginName + "*.dylib" );
#endif

#ifdef Q_WS_X11
    QPluginLoader loader (d->mPluginPrefix + pluginName + "*.so" );
#endif

#ifdef Q_WS_WIN
    QPluginLoader loader (d->mPluginPrefix + pluginName + "*.dll" );
#endif

    QObject *plugin = loader.instance();
    if (plugin) {
        AbstractPluginInterface *Iface = 0;
        Iface = qobject_cast<AbstractPluginInterface *> (plugin);
        d->mPluginGroups[pluginName] = Iface;
        qDebug() << "PluginLoader::load" << "Loading.." << Iface << pluginName << endl;

        const QStringList dictKeys = d->mDict.keys();
        if (!dictKeys.contains(interface)) {
            QStringList list;
            list << pluginName;
            d->mDict[interface] = list;
        } else {
            QStringList list;
            list = d->mDict[interface];
            list << pluginName;
            d->mDict[interface] = list;
        }

    } else {
        qDebug() << loader.errorString() << "Failed plugin: " << pluginName << endl;
    }
}

void PluginLoader::scanForPlugins()
{
    QDir dir(d->mPluginPrefix);
    dir.setFilter(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
    dir.setSorting(QDir::Size | QDir::Reversed);
    QFileInfoList list = dir.entryInfoList();
    for (int i = 0; i < list.size(); ++i) {
        QFileInfo fileInfo = list.at(i);
        loadDesktop(d->mPluginPrefix + fileInfo.fileName());
    }
}

void PluginLoader::setPluginPrefix(const QString &path)
{
    d->mPluginPrefix = path;
}

QString PluginLoader::pluginPrefix() const
{
    return d->mPluginPrefix;
}

void PluginLoader::loadDesktop(const QString &path)
{
    QSettings desktopFile(path, QSettings::IniFormat, this);
    desktopFile.beginGroup("Desktop Entry");
    load(desktopFile.value("Type").toString(), desktopFile.value("X-PLEXYDESK-Library").toString());
    desktopFile.endGroup();
}

}
