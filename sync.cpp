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

QList< action_t > handle_dir(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder)
{
    qDebug() << "requesting remote cache for " << remotefolder << " ...";
    const std::vector<remote_res_t> remote_entries = session.get_resources(remotefolder.toStdString());
    qDebug() << "    ok";

    
    qDebug() << "collecting local cache for " << localfolder << " ...";
    QFileInfoList local_entries;
    
    Q_FOREACH(const QFileInfo& info, QDir(localfolder).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
        if (info.fileName() == "." || info.fileName() == "..") continue;
        if (info.fileName() == db_t::prefix || info.suffix() == db_t::tmpprefix) continue;
            
        local_entries << info;
    };
    qDebug() << "    ok";
    
    enum filter {
        folders, files,
    };
    
    //helper function to obtain only file/filder names from complete LOCAL list
    auto justNames = [] (const QFileInfoList& list, filter f) {
        QSet<QString> names;
        Q_FOREACH(const auto& info, list) {
            if (info.fileName() == "." || info.fileName() == "..") continue;
            if (f == files && info.isDir() ) continue;
            if (f == folders && !info.isDir() ) continue;
            
            names << info.fileName();
        }
        return names;
    };
    
     //helper function to obtain only file/filder names from complete REMOTE list
    auto remoteNames = [] (const std::vector<remote_res_t>& list, filter f) {
        QSet<QString> names;
        Q_FOREACH(const auto& resource, list) {
            if (resource.name == "." || resource.name == "..") continue;    
            if (f == files && resource.dir) continue;
            if (f == folders && !resource.dir) continue;
            
            names << resource.name.c_str();
        }
        return names;
    };
    
    //helper function to find fileinfo by name
    auto find_info = [&local_entries] (const QString& file) {
        return std::find_if(local_entries.begin(), local_entries.end(), [&] (const QFileInfo& r) { return r.fileName() == file; });
    };
    
    //helper function to find resource by name
    auto find_resource = [&remote_entries] (const QString& file) {
        return std::find_if(remote_entries.begin(), remote_entries.end(), [&] (const remote_res_t& r) { return r.name == file.toStdString(); });
    };
    
    // Step 1 - making snapshot in filenames of local/remote 'filesystem'
    
    const QSet<QString> local_entries_set = justNames(local_entries, files);
    const QSet<QString> local_db_set = QSet<QString>::fromList(localdb.entries(localfolder));

    const QSet<QString> local_added = local_entries_set - local_db_set;
    const QSet<QString> local_deleted = local_db_set - local_entries_set;        
    const QSet<QString> local_exists = local_db_set & local_entries_set;
    
    const QSet<QString> remote_entries_set = remoteNames(remote_entries, files);
    const QSet<QString> remote_added = remote_entries_set - local_added - local_deleted - local_exists;
    
    qDebug() << "local_entries_set:" << QStringList(local_entries_set.toList()).join(";");
    qDebug() << "local_db_set:" << QStringList(local_db_set.toList()).join(";");
    qDebug() << "local_added:" << QStringList(local_added.toList()).join(";");
    qDebug() << "local_deleted:" << QStringList(local_deleted.toList()).join(";");
    qDebug() << "local_exists:" << QStringList(local_exists.toList()).join(";");    
    
    QList<action_t> actions;
    
    Q_FOREACH(const QString& file, local_added) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            qDebug() << "file " << file << " must be uploaded to server";
            auto fileinfo = find_info(file);
            Q_ASSERT(fileinfo != local_entries.end());
            actions.push_back(action_t(action_t::upload,
                localdb.entry(localfile),
                localfile,
                remotefile,
                *find_info(file),
                remote_res_t()));
        }
        else {
            qDebug() << "file " << file << " already exists on server - must be compared";
            auto fileinfo = find_info(file);
            auto resource = find_resource(file);
            Q_ASSERT(fileinfo != local_entries.end());
            Q_ASSERT(resource != remote_entries.end());
            actions.push_back(action_t(compare(localdb.entry(localfile), *fileinfo, *resource),
                localdb.entry(localfile),
                localfile,
                remotefile,
                *fileinfo,
                *resource));
        }
    }
    
    Q_FOREACH(const QString& file, local_deleted) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            qDebug() << "file " << file << " deleted and ON server too";
            actions.push_back(action_t(action_t::both_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                QFileInfo(),
                remote_res_t()));
        }
        else {
            qDebug() << "file " << file << " localy deleted must be compared with etag on server";
            auto resource = find_resource(file);          
            Q_ASSERT(resource != remote_entries.end());            
            actions.push_back(action_t(action_t::local_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                QFileInfo(),
                *resource));            
        }
    }
    
    Q_FOREACH(const QString& file, local_exists) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            auto fileinfo = find_info(file);
            Q_ASSERT(fileinfo != local_entries.end());            
            qDebug() << "file " << file << " deleted on server must be deleted localy";
            actions.push_back(action_t(action_t::remote_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                *fileinfo,
                remote_res_t()));            
        }
        else {
            qDebug() << "file " << file << " exists on server must be compared";
            auto fileinfo = find_info(file);
            auto resource = find_resource(file);                      
            Q_ASSERT(fileinfo != local_entries.end());   
            Q_ASSERT(resource != remote_entries.end());     
            actions.push_back(action_t(compare(localdb.entry(localfile), *fileinfo, *resource),
                localdb.entry(localfile),
                localfile,
                remotefile,
                *fileinfo,
                *resource));            
        }
    }
    
    Q_FOREACH(const QString& file, remote_added) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        qDebug() << "unhandler remote file:" << file << " must be downloaded";
        
        auto resource = find_resource(file);                      
        Q_ASSERT(resource != remote_entries.end());          
        actions.push_back(action_t(action_t::download,
            localdb.entry(localfile),
            localfile,
            remotefile,
            QFileInfo(),
            *resource));          
    }
        
        
    //recursion to directories
    
    QSet<QString> already_completed;
    
    Q_FOREACH(const QString& dir, justNames(local_entries, folders)) {
        const QString localdir = localfolder + "/" + dir;
        const QString remotedir = remotefolder + "/" + dir;
        
        auto resource = std::find_if(remote_entries.begin(), remote_entries.end(), [&] (const remote_res_t& r) { return r.name == dir.toStdString(); });
        
        if (resource == remote_entries.end()) {
            qDebug() << "dir " << dir << " must be uploaded to server";
            actions.push_back(action_t(action_t::upload_dir,
                db_entry_t(),
                localdir,
                remotedir,
                *find_info(dir),
                remote_res_t()));              
        }
        else if (resource->dir) {
            qDebug() << "dir " << dir << " already exists on server - normal recursion";
            try {
                actions << handle_dir(localdb, session, localdir, remotedir);
            } catch(const std::exception& e) {
                qCritical() << "Can't handle internal dir:" << e.what();
                actions.push_back(action_t(action_t::error,
                    db_entry_t(),
                    localdir,
                    remotedir,
                    *find_info(dir),
                    *resource));  
            }
        }
        else {
            qDebug() << "dir " << dir << " already exists on server - BUT NOT A DIR";
            actions.push_back(action_t(action_t::error,
                db_entry_t(),
                localdir,
                remotedir,
                *find_info(dir),
                *resource));              
        }
        
        already_completed << dir;
    }  
    
    Q_FOREACH(const QString& dir, remoteNames(remote_entries, folders)) {
        if (already_completed.contains(dir)) continue;
        
        const QString localdir = localfolder + "/" + dir;
        const QString remotedir = remotefolder + "/" + dir;
        
        auto localinfo = std::find_if(local_entries.begin(), local_entries.end(), [&] (const QFileInfo& r) { return r.fileName() == dir; });
        
        if (localinfo == local_entries.end()) {
            qDebug() << "dir " << dir << " must be downloaded from server";
            actions.push_back(action_t(action_t::download_dir,
                db_entry_t(),
                localdir,
                remotedir,
                QFileInfo(),
                *find_resource(dir)));              
        }
        else if (!localinfo->isDir()) {
            qDebug() << "dir " << dir << " already exists localy - BUT NOT A DIR";
            actions.push_back(action_t(action_t::error,
                localdb.entry(localdir),
                localdir,
                remotedir,
                *localinfo,
                *find_resource(dir)));               
        }
        else {
            Q_ASSERT(!"impossible");
        }
    }  
    
    return actions;
}

