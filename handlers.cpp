
#include <sys/stat.h> 
#include <fcntl.h> 

#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QProcess>

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

struct upload_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        try {
            auto resources = session.get_resources(action.remote_file);
        }
        catch (ne_exception_t& e) {
            return e.code() == 404;
        }
        return false;         
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        QFile file(action.local.absoluteFilePath());
        
        if (!file.open(QIODevice::ReadOnly)) 
            throw qt_exception_t(QString("Can't open file %1: %2").arg(action.local.absoluteFilePath()).arg(file.errorString()));
        
        stat_t stat = session.put(action.remote_file, file.handle());
        stat.perms = file.permissions();
        
        session.set_permissions(action.remote_file, stat.perms);
        
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

struct local_change_handler : upload_handler {
    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }    
};

struct download_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        return !QFileInfo(action.local_file).exists();
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        const QString tmppath = action.local_file + db_t::tmpprefix;
        
        QFile tmpfile(tmppath);
        if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
            throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg(tmpfile.errorString()));
        
        stat_t stat =  session.get(action.remote.path, tmpfile.handle());
        stat.perms = (action.remote.perms) ? action.remote.perms : tmpfile.permissions();
        tmpfile.setPermissions(stat.perms);

        const QFileInfo info(tmpfile);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();


        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path, stat);
        }
        
        QFile::remove(action.local_file);
        
        if (!tmpfile.rename(action.local_file))
            throw qt_exception_t(QString("Can't rename file %1 -> %2: %3").arg(tmppath).arg(action.local_file).arg(tmpfile.errorString()));   
        
        db_entry_t e = action.dbentry;
        e.stat = stat;
        db.save(e.folder + "/" + e.name, e);
    }
};

struct remote_change_handler : download_handler {
    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }    
};

struct conflict_handler : base_handler_t {
    action_processor_t::Resolver resolver_;
    action_processor_t::Comparer comparer_;
    
    conflict_handler(action_processor_t::Comparer c, action_processor_t::Resolver r) : comparer_(c), resolver_(r) {}

    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        const QString tmppath = action.local_file + db_t::tmpprefix;
        
        QFile tmpfile(tmppath);
        if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
            throw qt_exception_t(QString("Can't create file %1: %2").arg(tmppath).arg(tmpfile.errorString()));        
        
        tmpfile.setPermissions(action.local.permissions());
        
        stat_t stat =  session.get(action.remote.path, tmpfile.handle());
        stat.perms = tmpfile.permissions();
        
        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path, stat);
        }

        action_processor_t::resolve_ctx ctx = {
            action,
            action.local_file,
            tmppath                    
        };
        
        if (comparer_(ctx)) {
            qDebug() << "files are same!";
            stat.local_mtime = action.local.lastModified().toTime_t();
            stat.size = action.local.size();
            QFile::remove(tmppath); 
            
            session.set_permissions(action.remote_file, stat.perms);
            
            db_entry_t e = action.dbentry;
            e.stat = stat;
            db.save(e.folder + "/" + e.name, e);                       
        } 
        else {
            qDebug() << "files are differs : stat resolving...";
            if (resolver_(ctx)) {
                qDebug() << "Conflict resolved: result = " << ctx.result;
                
                QFile::remove(tmppath); 
                QFile::remove(action.local_file);
                
                if (!QFile::rename(ctx.result, action.local_file))
                    throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(ctx.result).arg(action.local_file));                       
                
                QFile::setPermissions(action.local_file, stat.perms);
                
                upload_handler()(session, db, action);                
            }
            else {
                qDebug() << "Can't resolve conflict!";
            }
        }
    }
};

struct both_deleted_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        try {
            auto resources = session.get_resources(action.remote_file);
        }
        catch (ne_exception_t& e) {
            return e.code() == 404 && !QFileInfo(action.local_file).exists();
        }
        return false;        
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        db.remove(action.local_file);
    }
};

struct local_deleted_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        session.remove(action.remote.path);
        db.remove(action.local_file);
    }
};

struct remote_deleted_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        try {
            auto resources = session.get_resources(action.remote_file);
        }
        catch (ne_exception_t& e) {
            return e.code() == 404;
        }
        return false;        
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        if (!QFile::remove(action.local_file))
            throw qt_exception_t(QString("Can't remove file %1").arg(action.local.absoluteFilePath()));
        
        db.remove(action.local.absoluteFilePath());
    }
};

struct upload_dir_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        
        stat_t stat = session.mkcol(action.remote_file);
        stat.perms = action.local.permissions();
        
        session.set_permissions(action.remote_file, stat.perms);
        
        stat.size = action.local.size();
        stat.local_mtime = action.local.lastModified().toTime_t();
        
        if (stat.etag.isEmpty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote_file, stat);
        }

        db_entry_t e = action.dbentry;
        
        e.dir = true;
        e.stat = stat;
        db.save(e.folder + "/" + e.name, e);        
        
        Q_FOREACH(const QFileInfo& info, QDir(action.local.absoluteFilePath()).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
            if (db_t::skip(info.fileName())) continue;

            action_t act(
                action_t::error,
                db.get_entry(info.absoluteFilePath()),
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
                upload_handler()(session, db, act);
            }
        }
    }
};

struct download_dir_handler : base_handler_t {
    bool do_check(session_t& session, const action_t& action) const {
        return true;
    }
    
    void do_request(session_t& session, db_t& db, const action_t& action) const {
        
        if (!QDir().mkdir(action.local_file))
            throw qt_exception_t(QString("Can't create dir: %1").arg(action.local_file));
        
        Q_FOREACH(const remote_res_t& resource, session.get_resources(action.remote.path)) {
            if (db_t::skip(resource.name)) continue;
            
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
                act.dbentry = db.get_entry(action.local_file + "/" + resource.name);
                download_handler()(session, db, act);
            }
        }
    }
};

action_processor_t::action_processor_t(session_t& session, db_t& db, Comparer comparer, Resolver resolver)
    : session_(session)
    , db_(db)
{
    handlers_ = {
        {action_t::upload, upload_handler()},           //FIXME check remote etag NOT exists - yandex bug
        {action_t::download, download_handler()},
        {action_t::local_changed, local_change_handler()},    //FIXME check remote etag NOT changed
        {action_t::remote_changed, remote_change_handler()},
        {action_t::conflict, conflict_handler(comparer, resolver)},
        {action_t::both_deleted, both_deleted_handler()},
        {action_t::local_deleted, local_deleted_handler()},  //FIXME check remote etag NOT changed
        {action_t::remote_deleted, remote_deleted_handler()},//FIXME check local NOT changed
        {action_t::upload_dir, upload_dir_handler()},
        {action_t::download_dir, download_dir_handler()},
        {action_t::unchanged, [] (session_t& session, db_t& db, const action_t& action) {}}
    };
}

bool action_processor_t::process(const action_t& action)
{
    auto h = handlers_.find(action.type);
    if (h != handlers_.end()) {
        qDebug() << "\n >>> processing action: " << action.type << " " << action.local_file << " --> " << action.remote_file;
        h->second(session_, db_, action);
        qDebug() << " >>> completed";
    }
    else {
        qCritical() << "unhandled action type!:" << action.type;
        Q_ASSERT(!"unhandled action type!");
    }
}

