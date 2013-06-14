
#include <sys/stat.h> 
#include <fcntl.h> 

#include <QFile>
#include <QDebug>
#include <QDateTime>

#include "handlers.h"

stat_t& update_head(session_t& session, const QString& remotepath, stat_t& stat) {
    std::string etag;
    time_t mtime;
    off_t size = 0;

    session.head(remotepath.toStdString(), etag, mtime, size);

    if (stat.etag.empty()) {
        stat.etag = etag;
    }
    else if (etag != stat.etag) {
        qDebug() << "etag differs " << stat.etag.c_str() << " remote:" << etag.c_str();
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
        
        stat_t stat = session.put(action.remote_file.toStdString(), file.handle());
        qDebug() << "Rtime:" << stat.remote_mtime;
        
        
        const QFileInfo info(file);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();
        
        if (stat.etag.empty() || stat.remote_mtime == 0) {
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
        
        stat_t stat =  session.get(action.remote.path.c_str(), tmpfile.handle());

        const QFileInfo info(tmpfile);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();


        if (stat.etag.empty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path.c_str(), stat);
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
        
        stat_t stat =  session.get(action.remote.path.c_str(), tmpfile.handle());

        const QFileInfo info(tmpfile);
        stat.size = info.size();
        stat.local_mtime = info.lastModified().toTime_t();


        if (stat.etag.empty() || stat.remote_mtime == 0) {
            stat = update_head(session, action.remote.path.c_str(), stat);
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
        {action_t::local_deleted, local_deleted_handler()},
        {action_t::remote_deleted, remote_deleted_handler()},        
    };
}

void action_processor_t::process(const action_t& action)
{

//         upload_dir    = 1 << 9,
//         download_dir  = 1 << 10,

    auto h = handlers_.find(action.type);
    if (h != handlers_.end()) {
        qDebug() << "\n >>> processing action: " << action.type << " " << action.local_file << " --> " << action.remote_file;
        h->second(session_, db_, action);
        qDebug() << " >>> completed";
    }
    else {
        qDebug() << "\n >>> SKIP action: " << action.type << " " << action.local_file << " --> " << action.remote_file;
    }
}

