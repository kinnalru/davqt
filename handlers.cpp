
#include <sys/stat.h> 
#include <fcntl.h> 

#include <QFile>
#include <QDebug>
#include <QDateTime>

#include "handlers.h"

stat_t& update_head(session_t& session, const QString& remotepath, stat_t& stat) {
    QString etag;
    time_t mtime;
    off_t size = 0;

    session.head(remotepath, etag, mtime, size);

    if (stat.etag.isEmpty()) {
        stat.etag = etag;
    }
    else if (etag != stat.etag) {
        qDebug() << "etag differs " << stat.etag << " remote:" << etag;
    }    
    
    if (stat.remote_mtime && stat.remote_mtime != mtime) {
        qDebug() << "remote mtime differs " << stat.remote_mtime << " remote:" << mtime;
        stat.remote_mtime = 0; //will be updated on next sync cycle
    }
    
    if (stat.size != size) {
        qDebug() << "remote size differs " << stat.size << " remote:" << size;
    }   
    return stat;
}

struct upload_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        QFile file(action.local.absoluteFilePath());
        
        if (!file.open(QIODevice::ReadOnly)) 
            throw qt_exception_t(QString("Can't open file %1 for reading").arg(action.local.absoluteFilePath()));
        
        stat_t stat = session.put(action.remote_file, file.handle());
        qDebug() << "Rtime:" << stat.remote_mtime;
        
        
        const QFileInfo info(file);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();
        
        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote_file, stat);
        }
        
        db_entry_t e = action.dbentry;
        e.stat = stat;
        db.save(e.folder + "/" + e.name, e);
    }
};

struct download_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        const QString tmppath = action.local_file + db_t::tmpprefix;
        
        QFile tmpfile(tmppath);
        if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
            throw qt_exception_t(QString("Can't create file %1").arg(tmppath));        
        
        stat_t stat =  session.get(action.remote.path, tmpfile.handle());

        const QFileInfo info(tmpfile);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();


        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path, stat);
        }
        
        QFile::remove(action.local_file);        
        if (!QFile::rename(tmppath, action.local_file))
            throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(tmppath).arg(action.local_file));   
        
        db_entry_t e = action.dbentry;
        e.stat = stat;
        db.save(e.folder + "/" + e.name, e);
    }
};

struct conflict_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        const QString tmppath = action.local_file + db_t::tmpprefix;
        
        QFile tmpfile(tmppath);
        if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
            throw qt_exception_t(QString("Can't create file %1").arg(tmppath));        
        
        stat_t stat =  session.get(action.remote.path, tmpfile.handle());

        const QFileInfo info(tmpfile);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();


        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path, stat);
        }
        
        //FIXME comapre and merge not realized
        {
            qDebug() << " !!! compare and merge not realized:" << tmppath << " and " << action.local_file;
//             if (!QFile::rename(tmppath, action.local_file))
//                 throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(tmppath).arg(action.local_file));   
//             
//             db_entry_t e = action.dbentry;
//             e.stat = stat;
//             
//             db.save(e.folder + "/" + e.name, e);
        }
    }
};

struct both_deleted_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        db.remove(action.local_file);
    }
};

struct local_deleted_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        session.remove(action.remote.path);
        db.remove(action.local_file);
    }
};

struct remote_deleted_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        if (!QFile::remove(action.local_file))
            throw qt_exception_t(QString("Can't remove file %1").arg(action.local.absoluteFilePath()));
        
        db.remove(action.local.absoluteFilePath());
    }
};

struct upload_dir_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        
        session.mkcol(action.remote_file);
        
        Q_FOREACH(const QFileInfo& info, QDir(action.local.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
            if (info.fileName() == "." || info.fileName() == "..") continue;
            if (info.fileName() == db_t::prefix || info.suffix() == db_t::tmpprefix) continue;

            action_t act(
                action_t::error,
                db_entry_t(),
                info.absoluteFilePath(),
                action.remote_file + "/" + info.fileName(),
                info,
                remote_res_t()
            );  
            
            if (info.isDir()) {
                act.type = action_t::upload_dir;
                (*this)(session, db, act);
            }
            else {
                act.type = action_t::upload;
                act.dbentry = db.entry(info.absoluteFilePath());
                upload_handler()(session, db, act);
            }
        }
    }
};

struct download_dir_handler {
    void operator() (session_t& session, db_t& db, const action_t& action) const {
        
        if (!QDir().mkdir(action.local_file))
            throw qt_exception_t(QString("Can't create dir: %1").arg(action.local_file));
        
        Q_FOREACH(const remote_res_t& resource, session.get_resources(action.remote.path)) {
            if (resource.name == ".") continue;
            
            action_t act(
                action_t::error,
                db_entry_t(),
                action.local_file + "/" + resource.name,
                resource.path,
                QFileInfo(),
                resource
            );              
            
            if (resource.dir) {
                act.type = action_t::download_dir;
                (*this)(session, db, act);                
            }
            else {
                act.type = action_t::download;
                act.dbentry = db.entry(action.local_file + "/" + resource.name);
                download_handler()(session, db, act);
            }
        }
    }
};

action_processor_t::action_processor_t(session_t& session, db_t& db)
    : session_(session)
    , db_(db)
{
    handlers_ = {
        {action_t::upload, upload_handler()},           //FIXME check remote etag NOT exists - yandex bug
        {action_t::download, download_handler()},
        {action_t::local_changed, upload_handler()},    //FIXME check remote etag NOT changed
        {action_t::remote_changed, download_handler()},
        {action_t::conflict, conflict_handler()},
        {action_t::both_deleted, both_deleted_handler()},
        {action_t::local_deleted, local_deleted_handler()},  //FIXME check remote etag NOT changed
        {action_t::remote_deleted, remote_deleted_handler()},//FIXME check local NOT changed
        {action_t::upload_dir, upload_dir_handler()},
        {action_t::download_dir, download_dir_handler()},
    };
}

bool action_processor_t::process(const action_t& action)
{
    auto h = handlers_.find(action.type);
    if (h != handlers_.end()) {
        qDebug() << "\n >>> processing action: " << action.type << " " << action.local_file << " --> " << action.remote_file;
        try {
            h->second(session_, db_, action);
            return true;
        } 
        catch (const std::exception& e) {
            qDebug() << " >>> error:" << e.what();
            return false;
        }
        qDebug() << " >>> completed";
    }
    else {
        qDebug() << ">>> SKIP action: " << action.type << " " << action.local_file << " --> " << action.remote_file;
    }
}

