
#include <sys/stat.h> 
#include <fcntl.h> 

#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QProcess>

#include "handlers.h"

namespace {
  QDebug debug() {
    return qDebug() << "[HANDLERS]:";
  }
}

struct upload_handler : base_handler_t {
  void do_check(session_t& session, const action_t& action) const {
    Q_ASSERT(!action.local.empty()); // local contais NEW "fixed" file stat to upload.
    Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity check
    
    try {
      auto resources = session.get_resources(action.key);
    }
    catch (ne_exception_t& e) {
      if (e.code() == 404) return;
    }
    
    throw std::runtime_error("Can't upload: file exists on server");
  }
  
  void do_request(session_t& session, database_p db, const action_t& action) const {
    const QString local_file = db->storage().absolute_file_path(action.key);
    const QString remote_file = db->storage().remote_file_path(action.key);
    
    QFile file(local_file);
    
    if (!file.open(QIODevice::ReadOnly)) 
      throw qt_exception_t(QString("Can't open file %1: %2").arg(action.key).arg(file.errorString()));
    
    debug() << "uploading file...";
    stat_t remotestat = session.put(remote_file, file.handle());
    debug() << "updating permissions...";
    remotestat = session.set_permissions(remote_file, action.local.perms);
    remotestat.perms = action.local.perms;

    if ( remotestat.mtime == 0 || remotestat.size == -1) {
        debug() << "additional HEAD request...";        
        session.head(remote_file, remotestat.mtime, remotestat.size);
    }
    
    debug() << "updating DB...";
    database::entry_t e = db->get(action.key);
    e.local = action.local;
    e.remote = remotestat;
    e.dir = false;
    e.bad = false;
    db->put(e.key, e);
    debug() << "finish";
  }
};

struct local_change_handler : upload_handler {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(!action.local.empty());  // local contais NEW "fixed" file stat to upload.
        Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity check
    }

    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
//         database::entry_t e = db.get(action.local.absolutepath);
//         if (e.local.size == action.local.size &&
//             e.local.mtime == action.local.mtime &&
//             e.local.perms != action.local.perms)
//         {
//             stat_t remotestat = session.set_permissions(action.remote_file, action.local.perms, db.get(action.local_file).remote.etag);
//             remotestat.perms = action.local.perms;
// 
//             if (remotestat.etag.isEmpty() || remotestat.mtime == 0 || remotestat.size == -1) {
//                 session.head(action.remote_file, remotestat.etag, remotestat.mtime, remotestat.size);
//             }
// 
//             e.local = action.local;
//             e.remote = remotestat;
//             e.dir = false;
//             e.bad = false;
//             db.put(e.name, e);
//         }
//         else {
//             upload_handler::do_request(session, db, action);
//         }
    }
};

struct download_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(action.local.empty());  // local MUST not exists - sanity check
        Q_ASSERT(!action.remote.empty());// remote contains stat do down load not "fixed"
//         if (QFileInfo(action.local_file).exists())
//             throw std::runtime_error("Can't download: file exists");
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
//         const QString tmppath = action.local_file + database::database_t::tmpprefix;
//         
//         if (QFileInfo(tmppath).exists())
//             throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg("file exists!"));
//         
//         QFile tmpfile(tmppath);
//         if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
//             throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg(tmpfile.errorString()));
//         
//         stat_t remotestat =  session.get(action.remote_file, tmpfile.handle());
//         remotestat.perms = action.remote.perms;
// 
//         if (remotestat.perms) {
//             tmpfile.setPermissions(remotestat.perms);
//         }
// 
//         if (remotestat.etag.isEmpty() || remotestat.mtime == 0 || remotestat.size == -1) {
//             session.head(action.remote_file, remotestat.etag, remotestat.mtime, remotestat.size);
//         }
// 
//         QFile::remove(action.local_file);
//         
//         if (!tmpfile.rename(action.local_file))
//             throw qt_exception_t(QString("Can't rename file %1 -> %2: %3").arg(tmppath).arg(action.local_file).arg(tmpfile.errorString()));   
// 
//         const stat_t localstat(action.local_file);
//         
//         db_entry_t e = db.get_entry(localstat.absolutepath);
//         e.local = localstat;
//         e.remote = remotestat;
//         e.dir = false;        
//         db.save(e.folder + "/" + e.name, e);
    }
};

struct remote_change_handler : download_handler {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(!action.local.empty()); // local MUST exists - sanity check
        Q_ASSERT(!action.remote.empty());// remote MUST exists - sanity check        
    }
};

struct conflict_handler : base_handler_t {
    action_processor_t::Resolver resolver_;
    action_processor_t::Comparer comparer_;
    
    conflict_handler(action_processor_t::Comparer c, action_processor_t::Resolver r) : comparer_(c), resolver_(r) {}

    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(!action.local.empty());  // local MUST exists - sanity check
        Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity check    
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
//         const QString tmppath = action.local_file + database::database_t::tmpprefix;
//         
//         QFile tmpfile(tmppath);
//         if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
//             throw qt_exception_t(QString("Can't create file %1: %2").arg(tmppath).arg(tmpfile.errorString()));        
// 
//         stat_t remotestat =  session.get(action.remote_file, tmpfile.handle());
//         remotestat.perms = action.remote.perms;
// 
//         if (remotestat.perms) {
//             tmpfile.setPermissions(remotestat.perms);
//         }
//         
//         if (remotestat.etag.isEmpty() || remotestat.mtime == 0 || remotestat.size == -1) {
//             session.head(action.remote_file, remotestat.etag, remotestat.mtime, remotestat.size);
//         }
// 
//         action_processor_t::resolve_ctx ctx = {
//             action,
//             action.local_file,
//             tmppath,
//             ""
//         };
//         
//         if (comparer_(ctx)) {
//             QFile::remove(tmppath); 
//             
//             const stat_t localstat(action.local_file);
//             remotestat = session.set_permissions(action.remote_file, localstat.perms, db.get_entry(action.local_file).remote.etag);
//             remotestat.perms = localstat.perms;
//             
//             if (remotestat.etag.isEmpty() || remotestat.mtime == 0 || remotestat.size == -1) {
//                 session.head(action.remote_file, remotestat.etag, remotestat.mtime, remotestat.size);
//             }
//             
//             db_entry_t e = db.get_entry(localstat.absolutepath);
//             e.local = localstat;
//             e.remote = remotestat;
//             e.dir = false;        
//             db.save(e.folder + "/" + e.name, e);                     
//         } 
//         else {
//             if (resolver_(ctx)) {
//                 QFile::remove(tmppath); 
//                 QFile::remove(action.local_file);
//                 
//                 if (!QFile::rename(ctx.result, action.local_file))
//                     throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(ctx.result).arg(action.local_file));                       
// 
//                 QFile::setPermissions(action.local_file, action.local.perms);                
//                 const stat_t localstat(action.local_file);
// 
//                 db_entry_t e = db.get_entry(localstat.absolutepath);
//                 e.local = localstat;
//                 e.remote = action.remote;
//                 e.dir = false;        
//                 e.bad = true;
//                 db.save(e.folder + "/" + e.name, e);  
//                 
//                 const action_t act(action_t::local_changed,
//                     action.local_file,
//                     action.remote_file,
//                     localstat,
//                     action.remote);
//                 
//                 local_change_handler()(session, db, act);
//             }
//             else {
//                 qDebug() << "Can't resolve conflict!";
//             }
//         }
    }
};

struct both_deleted_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(action.local.empty()); // local MUST not exists - sanity check
        Q_ASSERT(action.remote.empty());// remote MUST not exists - sanity check
        
//         try {
//             auto resources = session.get_resources(action.remote_file);
//         }
//         catch (ne_exception_t& e) {
//             if (e.code() == 404 && !QFileInfo(action.local_file).exists()) return;
//         }
//         
//         throw std::runtime_error("Can't delete BOTH: file exists on server or localy");
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
//         db.remove(action.local_file);
    }
};

struct local_deleted_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(action.local.empty());   // local MUST not exists - sanity check
        Q_ASSERT(!action.remote.empty()); // remote MUST exists - for ETag compare   
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
//         session.remove(action.remote.absolutepath, db.get_entry(action.local_file).remote.etag);
//         db.remove(action.local_file);
    }
};

struct remote_deleted_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(!action.local.empty()); // local MUST not exists - for changes compare 
        Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity
        
//         try {
//             auto resources = session.get_resources(action.remote_file);
//         }
//         catch (ne_exception_t& e) {
//             if (e.code() == 404) return;
//         }
//         throw std::runtime_error("Can't delete remote: file exists on server!");      
    }

    bool remove_dir(const QString & absolutepath) const
    {
// //         bool result;
// //         QDir dir(absolutepath);
// // 
// //         Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
// //             if (info.isDir()) {
// //                 result = remove_dir(info.absoluteFilePath());
// //             }
// //             else {
// //                 result = dir.remove(info.absoluteFilePath());
// //             }
// // 
// //             if (!result) {
// //                 return result;
// //             }
// //         }
// //         return dir.rmdir(absolutepath);
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
        
//         if (QFileInfo(action.local.absolutepath).isDir()) {
//             if (!remove_dir(action.local.absolutepath))
//                 throw qt_exception_t(QString("Can't remove dir %1").arg(action.local.absolutepath));
//         }
//         else {
//             if (!QFile::remove(action.local.absolutepath))
//                 throw qt_exception_t(QString("Can't remove file %1").arg(action.local.absolutepath));
//         }
//         
//         db.remove(action.local.absolutepath);
    }
};

struct upload_dir_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(!action.local.empty()); // local MUST exists - sanity
        Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
        
//         stat_t remotestat = session.mkcol(action.remote_file);
//         remotestat = session.set_permissions(action.remote_file, action.local.perms, remotestat.etag);
//         remotestat.perms = action.local.perms;
// 
//         remotestat.size = 0;
//         remotestat.mtime = 0;
//         remotestat.etag.clear();
// 
//         db_entry_t e = db.get_entry(action.local.absolutepath);
//         e.local = action.local;
//         e.remote = remotestat;
//         e.dir = true;
//         db.save(e.folder + "/" + e.name, e);        
//         
//         Q_FOREACH(const QFileInfo& info, QDir(action.local.absolutepath).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
//             if (database::database_t::skip(info.fileName())) continue;
// 
//             action_t act(
//                 action_t::error,
//                 info.absoluteFilePath(),
//                 action.remote_file + "/" + info.fileName(),
//                 stat_t(info),
//                 stat_t()
//             );  
//             
//             if (info.isDir()) {
//                 act.type = action_t::upload_dir;
//                 (*this)(session, db, act);
//             }
//             else {
//                 act.type = action_t::upload;
//                 upload_handler()(session, db, act);
//             }
//         }
    }
};

struct download_dir_handler : base_handler_t {
    void do_check(session_t& session, const action_t& action) const {
        Q_ASSERT(action.local.empty()); // local MUST not exists - sanity
        Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity
    }
    
    void do_request(session_t& session, database::database_t& db, const action_t& action) const {
        
//         if (!QDir().mkdir(action.local_file))
//             throw qt_exception_t(QString("Can't create dir: %1").arg(action.local_file));
// 
//         if (action.remote.perms) {
//             QFile::setPermissions(action.local_file, action.remote.perms);
//         }
// 
//         stat_t localstat(action.local_file);
//         localstat.size = 0;
//         localstat.mtime = 0;
//         localstat.etag.clear();
// 
//         db_entry_t e = db.get_entry(localstat.absolutepath);
//         e.local = localstat;
//         e.remote = action.remote;
//         e.dir = true;
//         db.save(e.folder + "/" + e.name, e);     
//         
//         Q_FOREACH(const remote_res_t& resource, session.get_resources(action.remote.absolutepath)) {
//             if (database::database_t::skip(resource.name)) continue;
//             
//             action_t act(
//                 action_t::error,
//                 action.local_file + "/" + resource.name,
//                 resource.path,
//                 stat_t(),
//                 stat_t(resource)
//             );              
//             
//             if (resource.dir) {
//                 act.type = action_t::download_dir;
//                 (*this)(session, db, act);                
//             }
//             else {
//                 act.type = action_t::download;
//                 download_handler()(session, db, act);
//             }
//         }
    }
};

action_processor_t::action_processor_t(session_t& session, database_p db, action_processor_t::Comparer comparer, action_processor_t::Resolver resolver)
    : session_(session)
    , db_(db)
{
    handlers_ = {
      {action_t::upload, upload_handler()},           //FIXME check remote etag NOT exists - yandex bug
//         {action_t::download, download_handler()},
//         {action_t::local_changed, local_change_handler()},    //FIXME check remote etag NOT changed
//         {action_t::remote_changed, remote_change_handler()},
//         {action_t::conflict, conflict_handler(comparer, resolver)},
//         {action_t::both_deleted, both_deleted_handler()},
//         {action_t::local_deleted, local_deleted_handler()},  //FIXME check remote etag NOT changed
//         {action_t::remote_deleted, remote_deleted_handler()},//FIXME check local NOT changed
//         {action_t::upload_dir, upload_dir_handler()},
//         {action_t::download_dir, download_dir_handler()},
//         {action_t::unchanged, [] (session_t& session, database::database_t& db, const action_t& action) {}},
//         {action_t::error, [] (session_t& session, database::database_t& db, const action_t& action) {
//             throw qt_exception_t(QObject::tr("Action precondition failed"));
//         }}
    };
}

void action_processor_t::process(const action_t& action)
{
//     Q_ASSERT(!action.empty());
    
  auto h = handlers_.find(action.type);
  if (h != handlers_.end()) {
      debug() << QString("processing action: %1 [%2]").arg(action_t::type_text(action.type)).arg(action.key);
      h->second(session_, db_, action);
      debug() << "completed";
  }
  else {
//         qCritical() << "unhandled action type!:" << action_t::type_text(action.type);
//         Q_ASSERT(!"unhandled action type!");
  }
}

