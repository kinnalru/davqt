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

