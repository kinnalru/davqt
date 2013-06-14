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

#include <QDateTime>
#include <QDebug>

#include "sync.h"

const QString etag_c = "etag";
const QString local_mtime_c = "local_mtime";
const QString remote_mtime_c = "remote_mtime";
const QString size_c = "size";

const QString db_t::prefix = ".davqt";
const QString db_t::tmpprefix = ".davtmp";

db_t::db_t(const QString& dbpath, const QString& localroot)
    : dbpath_(dbpath)
    , localroot_(localroot)
{
    auto s = settings();
    
    qDebug() << "DB: loading files:" << s->childGroups();
    
    
    Q_FOREACH(const QString& escaped_folder, s->childGroups()) {
        const QString folder = QString(escaped_folder).replace("===", "/"); 
        auto& dbfolder = db_[folder];
        
        s->beginGroup(escaped_folder);
        Q_FOREACH(const QString& file, s->childGroups()) {
            s->beginGroup(file);
            dbfolder[file] = db_entry_t(
                localroot_.absolutePath(),
                folder,
                file,
                s->value(etag_c).toString().toStdString(),
                s->value(local_mtime_c).toLongLong(),
                s->value(remote_mtime_c).toLongLong(),
                s->value(size_c).toULongLong()
            );
            s->endGroup();
        }
        s->endGroup();        
    }
}

void db_t::save(const QString& key, const db_entry_t& e)
{
    auto s = settings();
    
    const QFileInfo relative = QFileInfo(localroot_.relativeFilePath(key));
    const QString folder = relative.path();
    const QString escaped_folder = relative.path().replace("/", "===");

    auto& dbfolder = db_[folder];
    db_entry_t& sv = dbfolder[relative.fileName()];
    
    if (!e.empty()) {
        Q_ASSERT(e.folder == relative.path());
        Q_ASSERT(e.name == relative.fileName());
        sv = e;
    }
    
    qDebug() << "DB: save file:" << folder << " name:" << relative.fileName()  << " raw:" << key;
     
    s->beginGroup(escaped_folder);
        s->beginGroup(sv.name);
        s->setValue(etag_c, sv.stat.etag.c_str());
        s->setValue(local_mtime_c, qlonglong(sv.stat.local_mtime));
        s->setValue(remote_mtime_c, qlonglong(sv.stat.remote_mtime));
        s->setValue(size_c, qulonglong(sv.stat.size));
        s->endGroup();
    s->endGroup();
}

void db_t::remove(const QString& key)
{
    auto s = settings();
    const QFileInfo relative = QFileInfo(localroot_.relativeFilePath(key));
    const QString folder = relative.path();
    const QString escaped_folder = relative.path().replace("/", "===");

    auto& dbfolder = db_[folder];
    db_entry_t& sv = dbfolder[relative.fileName()];
    
    qDebug() << "DB: DELETE file:" << folder << " name:" << relative.fileName()  << " raw:" << key;
    s->beginGroup(escaped_folder);
    s->remove(sv.name);
    s->endGroup();
    
    dbfolder.erase(relative.fileName());
}


action_t::type_e compare(const db_entry_t& dbentry, const local_res_t& local, const remote_res_t& remote)
{
    qDebug() << "comparing :" << local.absoluteFilePath() << " <-> " << remote.path.c_str();
    action_t::TypeMask mask = 0;
    
    if (QDateTime::fromTime_t(dbentry.stat.local_mtime) != local.lastModified()) {
        mask |= action_t::local_changed;
        qDebug() << " -> local time changed:" << QDateTime::fromTime_t(dbentry.stat.local_mtime) << " -> " << local.lastModified();
    }
    
    if (dbentry.stat.size != local.size()) {
        mask |= action_t::local_changed;
        qDebug() << " -> local size changed:" << dbentry.stat.size << " -> " << local.size();
    }
    
    if (QDateTime::fromTime_t(dbentry.stat.remote_mtime) != QDateTime::fromTime_t(remote.mtime)) {
        mask |= action_t::remote_changed;
        qDebug() << " -> remote time changed:" << QDateTime::fromTime_t(dbentry.stat.remote_mtime) << " -> " << QDateTime::fromTime_t(remote.mtime);
    }
    
    if (dbentry.stat.size != remote.size) {
        mask |= action_t::remote_changed;
        qDebug() << " -> remote size changed:" << dbentry.stat.size << " -> " << remote.size;
    }
    
    if (dbentry.stat.etag != remote.etag) {
        mask |= action_t::remote_changed;
        qDebug() << " -> etag changed:" << dbentry.stat.etag.c_str() << " -> " << remote.etag.c_str();
    }
    
    if (mask == action_t::local_changed) return action_t::local_changed;
    if (mask == action_t::remote_changed) return action_t::remote_changed;
    
    if (mask & action_t::local_changed && mask & action_t::remote_changed) {
        qDebug() << " -> CONFLICT";
        return action_t::conflict;
    }
    
    qDebug() << " -> UNCHANGED";
    action_t::unchanged;
}

