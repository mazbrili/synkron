/*******************************************************************
 This file is part of Synkron
 Copyright (C) 2005-2011 Matus Tomlein (matus.tomlein@gmail.com)

 Synkron is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public Licence
 as published by the Free Software Foundation; either version 2
 of the Licence, or (at your option) any later version.

 Synkron is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public Licence for more details.

 You should have received a copy of the GNU General Public Licence
 along with Synkron; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
********************************************************************/

#ifndef ABSTRACTSYNCPAGE_H
#define ABSTRACTSYNCPAGE_H

class Folders;
class Folder;
class Settings;
class ExceptionBundle;
class SyncExceptionBundle;

#include <QVariant>

class AbstractSyncPage : public QObject
{
     Q_OBJECT

public:
    AbstractSyncPage(int);

    void save(Settings *);
    void load(Settings *);

    int index() { return id; }

    Folder * addFolder(int);
    void closeFolder(int);

    Folders * foldersObject() { return folders; }

    void setValue(const QString &, const QVariant &);
    const QVariant value(const QString &);

    QList<int> exceptionBundleIds();
    bool exceptionBundleChecked(int);
    void checkExceptionBundle(int, bool);
    ExceptionBundle * exceptionBundle(int);
    SyncExceptionBundle * syncExceptionBundle();

public slots:
    void addExceptionBundle(ExceptionBundle *);
    void removeExceptionBundle(int);
    void changeExceptionBundle(ExceptionBundle *);

protected:
    int id;
    Folders * folders;

    QMap<QString, QVariant> * settings_map;
    QMap<int, bool> * exception_bundle_ids_map;
    QMap<int, ExceptionBundle *> * exception_bundles_map;

signals:
    void exceptionBundleAdded(ExceptionBundle *);
    void exceptionBundleRemoved(int);
    void exceptionBundleChanged(ExceptionBundle *);
};

#endif // ABSTRACTSYNCPAGE_H