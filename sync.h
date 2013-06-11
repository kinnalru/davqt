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

#include <QString>
#include <QStringList>
#include <QSettings>
#include <QMap>
#include <map>

// class ne_exception_t : public std::runtime_error {
//     int error;
// public:
//     explicit ne_exception_t(int error);
//     ~ne_exception_t() throw() {};
// 
//     int error() const;
// };


enum state_e {
    exist,
    absent,
    added,
    deleted,
};

struct db_entry_t {
    QString root;
    QString path;
    std::string etag;
    
    time_t local_mtime;
    time_t remote_mtime;
    
    off_t size;
};

struct db_t {

    state_e state(const QString& path, bool currently_exist) {
        auto it = db_.find(path);
        
        if (it != db_.end()) {
            if (currently_exist)
                return exist;
            else 
                return deleted;
        }
        else {
            if (currently_exist)
                return added;
            else 
                return absent;
        }
    }
    
    void load(const QString& path) {
        db_.clear();
        
        QSettings s(path, QSettings::IniFormat);
        Q_FOREACH(QString key, s.allKeys()) {
            db_[key].path = key;
        }
    }
    
    db_entry_t& entry(const QString& path) {
        return db_[path];
    }
    
    QStringList entries() const {
        return QMap<QString, db_entry_t>(db_).keys();
    }
    
    std::map<QString, db_entry_t> db_;
};

class sync_t
{
    sync_t( const QString& path1, const QString& path2) {
        db_t db;
        db.load(path1 + "/.davqt/db");
    }
    
};

#endif // SYNC_H
