
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
  void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
    Q_ASSERT(!action.local.empty()); // local contais NEW "fixed" file stat to upload.
    Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity check
    
//     try {
//       auto resources = webdav.list(action.key);
//     }
//     catch (ne_exception_t& e) {
//       if (e.code() == 404) return;
//     }
//     
//     throw std::runtime_error("Can't upload: file exists on server");
  }
  
  Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
    qDebug() << Q_FUNC_INFO;
    
    const QString local_file = db->storage().absolute_file_path(action.key);
    const QString remote_file = db->storage().remote_file_path(action.key);
    
    QFile file(local_file);
    
    if (!file.open(QIODevice::ReadOnly)) 
      throw qt_exception_t(QString("Can't open file %1: %2").arg(action.key).arg(file.errorString()));
    
    stat_t remotestat;
    QReplyWaiter w1(webdav.put(remote_file, &file), true);
//     remotestat.merge(
    QReplyWaiter w2(webdav.setPermissions(remote_file, action.local.perms), true);
//     );
    remotestat.perms = action.local.perms;

    if (remotestat.empty()) {
        remotestat = stat_t(webdav.parse(webdav.list(remote_file)).first());
    }
    
    database::entry_t e = db->create(action.key, action.local, remotestat, false);
    db->put(e.key, e);
    return Actions();
  }
};

struct download_handler : base_handler_t {
  void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
//     Q_ASSERT(action.local.empty());  // local MUST not exists - sanity check
    Q_ASSERT(!action.remote.empty());// remote contains stat do down load not "fixed"
//     if (QFileInfo(db->storage().absolute_file_path(action.key)).exists())
//       throw std::runtime_error("Can't download: file exists");
  }
  
  Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
    qDebug() << Q_FUNC_INFO;
    
    const QString local_file = db->storage().absolute_file_path(action.key);
    const QString remote_file = db->storage().remote_file_path(action.key);      
    
    const QString tmppath = local_file + storage_t::tmpsuffix;
    
    if (QFileInfo(tmppath).exists())
      throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg("file exists!"));
    
    QFile tmpfile(tmppath);
    if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
      throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg(tmpfile.errorString()));
    
    stat_t remotestat;
    QReplyWaiter w1(webdav.get(remote_file, &tmpfile), true);
    remotestat.perms = action.remote.perms;

    if (remotestat.perms) {
      tmpfile.setPermissions(remotestat.perms);
    }

    if (remotestat.empty()) {
      remotestat = stat_t(webdav.parse(webdav.list(remote_file)).first());
    }

    QFile::remove(local_file);
    
    if (!tmpfile.rename(local_file))
      throw qt_exception_t(QString("Can't rename file %1 -> %2: %3").arg(tmppath).arg(local_file).arg(tmpfile.errorString()));   

    const stat_t localstat(local_file);
    
    database::entry_t e = db->create(action.key, localstat, remotestat, false);
    db->put(e.key, e);
    
    return Actions();
  }
};

struct forgot_handler : base_handler_t {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
        Q_ASSERT(action.local.empty()); // local MUST not exists - sanity check
        Q_ASSERT(action.remote.empty());// remote MUST not exists - sanity check
    }
    
    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
        qDebug() << Q_FUNC_INFO;
        
        db->remove(action.key);
        return Actions();
    }
};

struct local_change_handler : upload_handler {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
//         Q_ASSERT(!action.local.empty());  // local contais NEW "fixed" file stat to upload.
        Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity check
    }

    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
        qDebug() << Q_FUNC_INFO;
        
        database::entry_t e = db->get(action.key);
        if (e.local.size == action.local.size &&
            e.local.mtime == action.local.mtime &&
            e.local.perms != action.local.perms)
        {
            const QString local_file = db->storage().absolute_file_path(action.key);
            const QString remote_file = db->storage().remote_file_path(action.key);      
            
            stat_t remotestat;
            QReplyWaiter w1(webdav.setPermissions(remote_file, action.local.perms), true);
            remotestat.perms = action.local.perms;

            if (remotestat.empty()) {
              remotestat = stat_t(webdav.parse(webdav.list(remote_file)).first());
            }

            database::entry_t e = db->create(action.key, action.local, remotestat, false);
            db->put(e.key, e);
        }
        else {
            upload_handler::do_request(webdav, db, action);
        }
        return Actions();
    }
};

struct remote_change_handler : download_handler {
    void do_check(QWebdav& webdav, const action_t& action) const {
        Q_ASSERT(!action.local.empty()); // local MUST exists - sanity check
        Q_ASSERT(!action.remote.empty());// remote MUST exists - sanity check        
    }
};

struct conflict_handler : base_handler_t {
  action_processor_t::Resolver resolver_;
  action_processor_t::Comparer comparer_;
  
  conflict_handler(action_processor_t::Comparer c, action_processor_t::Resolver r) : comparer_(c), resolver_(r) {}

  void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
    Q_ASSERT(!action.local.empty());  // local MUST exists - sanity check
    Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity check    
  }
  
  Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
    qDebug() << Q_FUNC_INFO;
    
    
    const QString local_file = db->storage().absolute_file_path(action.key);
    const QString remote_file = db->storage().remote_file_path(action.key);      

    Q_ASSERT(!QFileInfo(local_file).isDir());
    
    const QString tmppath = local_file + storage_t::tmpsuffix;
      
    QFile tmpfile(tmppath);
    if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
      throw qt_exception_t(QString("Can't create file %1: %2").arg(tmppath).arg(tmpfile.errorString()));        

    stat_t remotestat;
    QReplyWaiter w1(webdav.get(remote_file, &tmpfile), true);
    remotestat.perms = action.remote.perms;

    if (remotestat.perms) {
      tmpfile.setPermissions(remotestat.perms);
    }
    
    if (remotestat.empty()) {
//       remotestat.merge(webdav.head(remote_file));
      remotestat = stat_t(webdav.parse(webdav.list(remote_file)).first());
    }
    
    action_processor_t::resolve_ctx ctx = {
      action,
      local_file,
      tmppath,
      ""
    };
    
    if (comparer_(ctx)) {
      //File contents is same
      QFile::remove(tmppath); 
      
      const stat_t localstat(local_file);
//       remotestat.merge(
        QReplyWaiter w1(webdav.setPermissions(remote_file, localstat.perms), true);
//       );
      remotestat.perms = localstat.perms;
      
      if (remotestat.empty()) {
//         remotestat.merge(session.head(remote_file));
        remotestat = stat_t(webdav.parse(webdav.list(remote_file)).first());
      }
      
      database::entry_t e = db->create(action.key, localstat, remotestat, false);
      db->put(e.key, e);
    } 
    else {
      //File contents differs
      if (resolver_(ctx)) {
        //Files merged
        QFile::remove(tmppath); 
        QFile::remove(local_file);
        
        if (!QFile::rename(ctx.result, local_file))
          throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(ctx.result).arg(local_file));                       

        QFile::setPermissions(local_file, action.local.perms);                
        const stat_t localstat(local_file);

        database::entry_t e = db->create(action.key, localstat, remotestat, false);
        db->put(e.key, e);
        
        const action_t act(action_t::local_changed,
          action.key,
          localstat,
          remotestat);
        
        local_change_handler()(webdav, db, act);
      }
      else {
        qDebug() << "Can't resolve conflict!";
      }
    }
    
    return Actions();
  }
};

struct remove_local_handler : base_handler_t {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
        Q_ASSERT(!action.local.empty());   // local MUST exists - sanity check
        Q_ASSERT(action.remote.empty()); // remote MUST not exists - for ETag compare   
    }
    
    bool remove_dir(const QString & path) const
    {
        bool result;
        QDir dir(path);

        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files, QDir::DirsFirst)) {
            if (info.isDir()) {
                result = remove_dir(info.absoluteFilePath());
            }
            else {
                result = dir.remove(info.absoluteFilePath());
            }

            if (!result) {
                return result;
            }
        }
        return dir.rmdir(path);
    }
    
    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
        qDebug() << Q_FUNC_INFO << action;
        const QString local_file = db->storage().absolute_file_path(action.key);
        
        if (QFileInfo(local_file).isDir()) {
            if (!remove_dir(local_file))
                throw qt_exception_t(QString("Can't remove dir %1").arg(local_file));
        }
        else {
            if (!QFile::remove(local_file))
                throw qt_exception_t(QString("Can't remove file %1").arg(local_file));
        }

        db->remove(action.key);
        return Actions();
    }
};

struct remove_remote_handler : base_handler_t {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
        Q_ASSERT(action.local.empty()); // local MUST not exists - for changes compare 
//         Q_ASSERT(!action.remote.empty()); // remote MUST exists - sanity
    
    }


    
    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
        qDebug() << Q_FUNC_INFO;
        const QString remote_file = db->storage().remote_file_path(action.key);
        
        QReplyWaiter w1(webdav.remove(remote_file), true);
        db->remove(action.key);
        return Actions();
    }
};

struct upload_dir_handler : base_handler_t {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
        Q_ASSERT(!action.local.empty()); // local MUST exists - sanity
        Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity
    }
    
    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
      qDebug() << Q_FUNC_INFO;
      const QString local_file = db->storage().absolute_file_path(action.key);
      const QString remote_file = db->storage().remote_file_path(action.key);
      
      stat_t remotestat;
      QReplyWaiter w1(webdav.mkcol(remote_file), true);
//       remotestat.merge(
        QReplyWaiter w2(webdav.setPermissions(remote_file, action.local.perms), true);
//       );
      remotestat.perms = action.local.perms;

      remotestat.size = 0;
      remotestat.mtime = 0;

      database::entry_t e = db->create(action.key, action.local, remotestat, true);
      db->put(e.key, e);
      
      Actions result;
      
      Q_FOREACH(const QFileInfo& info, QDir(local_file).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden | QDir::System)) {
        action_t act(
          action_t::error,
          db->key(info.absoluteFilePath()),
          stat_t(info),
          stat_t()
        );  
        
        if (info.isDir()) {
          act.type = action_t::upload_dir;
        }
        else {
          act.type = action_t::upload;
        }
        
        result << act;
      }
      return result;
    }
};

struct download_dir_handler : base_handler_t {
    void do_check(QWebdav& webdav, database_p& db, const action_t& action) const {
        Q_ASSERT(action.local.empty()); // local MUST not exists - sanity
        Q_ASSERT(action.remote.mtime || (action.remote.size == -1)); // remote MUST exists - sanity
    }
    
    Actions do_request(QWebdav& webdav, database_p& db, const action_t& action) const {
        qDebug() << Q_FUNC_INFO;
        
        const QString local_file = db->storage().absolute_file_path(action.key);
        const QString remote_file = db->storage().remote_file_path(action.key);
        
        if (!QFileInfo(local_file).exists() && !QDir().mkdir(local_file))
            throw qt_exception_t(QString("Can't create dir: %1").arg(local_file));

        if (action.remote.perms) {
            QFile::setPermissions(local_file, action.remote.perms);
        }

        stat_t localstat(local_file);
        localstat.size = 0;
        localstat.mtime = 0;

        database::entry_t e = db->create(action.key, localstat, action.remote, true);
        db->put(e.key, e);
        
        Actions result;
        
        Q_FOREACH(const QWebdavUrlInfo& resource, webdav.parse(webdav.list(remote_file))) {
            
            if (action.key == db->key(resource.name())) continue;
            
            action_t act(
                action_t::error,
                db->key(resource.name()),
                stat_t(),
                stat_t(resource)
            );              
            
            if (resource.isDir()) {
              act.type = action_t::download_dir;
            }
            else {
              act.type = action_t::download;              
            }
            result << act;
        }
        return result;
    }
};

action_processor_t::action_processor_t(QWebdav& webdav, database_p db, action_processor_t::Comparer comparer, action_processor_t::Resolver resolver)
    : webdav_(webdav)
    , db_(db)
{
    
    handlers_ = {
        {action_t::upload,   upload_handler()},
        {action_t::download, download_handler()},
        {action_t::forgot,   forgot_handler()},
        {action_t::remove_local,   remove_local_handler()},
        {action_t::remove_remote,   remove_remote_handler()},
        {action_t::local_changed, local_change_handler()},
        {action_t::remote_changed, remote_change_handler()},
        
        {action_t::upload_dir, upload_dir_handler()},
        {action_t::download_dir, download_dir_handler()},
        {action_t::unchanged, [] (QWebdav& webdav, database_p db, const action_t& action) {return Actions();}},
        {action_t::conflict, conflict_handler(comparer, resolver)},
        {action_t::error, [this] (QWebdav& webdav, database_p db, const action_t& action) {
            throw qt_exception_t(QObject::tr("Action precondition failed"));
            return Actions();
        }}
    };
}

void action_processor_t::process(const action_t& action)
{
    auto h = handlers_.find(action.type);
    if (h != handlers_.end()) {
        debug() << QString("processing action: %1 [%2]").arg(action.type_text()).arg(action.key);
        Actions actions = h->second(webdav_, db_, action);
        if (!actions.isEmpty()) {
          Q_EMIT new_actions(actions);
        }
        debug() << "completed";
    }
    else {
        qCritical() << "unhandled action type!:" << action;
        Q_ASSERT(!"unhandled action type!");
    }
}

