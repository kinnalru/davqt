/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef SYNC_H
#define SYNC_H

#include <map>
#include <memory>

#include <QDir>
#include <QMap>
#include <QSettings>
#include <QStringList>
#include <QDebug>

#include "types.h"


struct db_t {

    static const QString prefix;
    static const QString tmpprefix;
    
    db_t(const QString& dbpath, const QString& localroot);
   
    void save(const QString& key, const db_entry_t& e = db_entry_t());
    
    void remove(const QString& key);
    
    inline db_entry_t& entry(const QString& absolutepath) {
        const QFileInfo relative = QFileInfo(localroot_.relativeFilePath(absolutepath));

        db_entry_t& e = db_[relative.path()][relative.fileName()];
        e.folder = relative.path();
        e.name = relative.fileName();
        qDebug() << "get entry:" << e.folder << " name:" << e.name;
        return e;
    }
    
    inline QStringList entries(QString folder) const {
        if (localroot_.absolutePath() == folder)
            folder = ".";
        else
            folder = localroot_.relativeFilePath(folder);

        auto it = db_.find(folder);
        return (it == db_.end())
            ? QStringList()
            : QMap<QString, db_entry_t>(it->second).keys();
    }
    
private:
    inline std::unique_ptr<QSettings> settings() {
        return std::unique_ptr<QSettings>(new QSettings(dbpath_, QSettings::IniFormat));
    }
    
    QString dbpath_;
    QDir localroot_;
    
    typedef std::map<QString, db_entry_t> FolderDb;
    std::map<QString, FolderDb> db_;
};


action_t::type_e compare(const db_entry_t& dbentry, const local_res_t& local, const remote_res_t& remote);





#endif // SYNC_H
