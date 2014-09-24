
#include "tools.h"
#include "updater.h"


struct updater_t::Pimpl {
    database_p db;
    connection_t conn;
    bool stop;
    
    std::map<QString, remote_res_t>      remote_cache;
    std::map<QString, QFileInfo>         local_cache;
    std::map<QString, database::entry_t> db_cache;
};

updater_t::updater_t(database_p db, const connection_t& connection)
  : QObject(0)
  , p_(new Pimpl)
{
  p_->db = db;
  p_->conn = connection;
  p_->stop = false;
}

updater_t::~updater_t()
{
}

void updater_t::start()
{
  Q_EMIT started();

  try {
    session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
    {
      if (p_->stop) {
        Q_EMIT finished();
        return;
      }
      Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));
    }
    
    session.set_auth(p_->conn.login, p_->conn.password);
    session.set_ssl();
    session.open();
    
    qDebug() << Q_FUNC_INFO << "Updating state...";
    p_->remote_cache.clear();
    p_->local_cache.clear();
    p_->db_cache.clear();
    
    const Actions actions = fill(update(session, p_->db->storage().prefix(), p_->db->storage().path()));
    {
      qDebug() << Q_FUNC_INFO << actions;
      Q_EMIT status(actions);

//       QMutexLocker locker(&p_->mx);        
//       if (!p_->stop) {
//           qDebug() << p_->thread_name.localData() << Q_FUNC_INFO << "Scanning and comparing...";
//           Q_EMIT status_updated(actions);
//       }
//       qDebug() << p_->thread_name.localData() << "upd12";
//       Q_ASSERT(p_->in_progress.isEmpty());
//       Q_ASSERT(p_->todo.isEmpty());
//       p_->updating = false;
    }
  }
  catch (const std::exception& e) {
      qDebug() << "Exception in " << Q_FUNC_INFO << ": " << e.what();
//       QMutexLocker locker(&p_->mx);        
//       if (!p_->stop)
//           Q_EMIT status_error(e.what());
//       
//       Q_ASSERT(p_->in_progress.isEmpty());
//       Q_ASSERT(p_->todo.isEmpty());
//       p_->updating = false;
  }
 
  Q_EMIT finished();
 
}

void updater_t::stop()
{
  p_->stop = true;
}

typedef QSet<QString> Files;

struct snapshot_data {
  
  snapshot_data(const Files& db, const Files& local, const Files& remote)
    : db_files(db)
    , local_files(local)
    , remote_files(remote)
  {
    added_to_local = local_files - db_files;
    removed_from_local = db_files - local_files;
    stay_local = db_files & local_files;
    exists_local = stay_local + added_to_local;
    
    added_to_remote = remote_files - db_files;
    removed_from_remote = db_files - remote_files;
    stay_remote = db_files & remote_files;
    exists_remote = stay_remote + added_to_remote;

    to_remove_local = removed_from_remote & exists_local;
    to_remove_remote = removed_from_local & exists_remote;
    
    to_upload   = (exists_local - to_remove_local) - (exists_remote - to_remove_remote);
    to_download = (exists_remote - to_remove_remote) - (exists_local - to_remove_local);
    to_compare  = exists_local & exists_remote;
    to_forgot   = removed_from_local & removed_from_remote;
    
    qDebug() << "ExR:" << exists_remote;
    qDebug() << "ExL:" << exists_local;
    qDebug() << "Dw:" << to_download;
    
    Q_ASSERT((to_upload & to_download).isEmpty());
    Q_ASSERT((to_forgot & exists_local).isEmpty());
    Q_ASSERT((to_forgot & exists_remote).isEmpty());
    
    Q_ASSERT((to_remove_local & to_upload).isEmpty());
    Q_ASSERT((to_remove_local & to_compare).isEmpty());
    Q_ASSERT((to_remove_local & to_download).isEmpty());
    
    Q_ASSERT((to_remove_remote & to_upload).isEmpty());
    Q_ASSERT((to_remove_remote & to_compare).isEmpty());
    Q_ASSERT((to_remove_remote & to_download).isEmpty());
  }
  
  const Files db_files;
  const Files local_files;
  const Files remote_files;
  
  Files added_to_local;
  Files added_to_remote;
  
  Files removed_from_local;
  Files removed_from_remote;
  
  Files stay_local; //files on disk AND in cache
  Files stay_remote;//files at remote AND in cache
  
  Files exists_local; //all files on disk
  Files exists_remote;//all files at remote
  
  Files to_upload;
  Files to_compare;
  Files to_download;
  Files to_forgot;
  Files to_remove_local;
  Files to_remove_remote;
};

QList<action_t> updater_t::update(session_t& session, const QString& l, const QString& rf)
{
    const QString lf = p_->db->key(l);
    qDebug() << Q_FUNC_INFO << "Local:" << lf << " Remote:" << rf;
    
    const auto remotes = session.get_resources(rf);
    const auto remote_storage = std::accumulate(remotes.begin(), remotes.end(),
      std::map<QString, remote_res_t>(),
      [&] (std::map<QString, remote_res_t>& result, const remote_res_t& resource) {
        
        if (resource.name != ".") {
          result[p_->db->key(resource.path)] = resource;
        }

        return result;
      });
    p_->remote_cache.insert(remote_storage.begin(), remote_storage.end());
    
    const auto locals = p_->db->storage().entries(lf);
    const auto local_storage = std::accumulate(locals.begin(), locals.end(),
      std::map<QString, QFileInfo>(),
      [&] (std::map<QString, QFileInfo>& result, const QFileInfo& resource) {
        result[p_->db->key(resource.absoluteFilePath())] = resource;
        return result;
      });
    p_->local_cache.insert(local_storage.begin(), local_storage.end());    

    const auto dbs = p_->db->entries(lf);
    const auto db_storage = std::accumulate(dbs.begin(), dbs.end(),
      std::map<QString, database::entry_t>(),
      [&] (std::map<QString, database::entry_t>& result, const database::entry_t& resource) {
        result[resource.key] = resource;
        return result;
      });  
    p_->db_cache.insert(db_storage.begin(), db_storage.end());        
    
    QList<action_t> blank_actions;
    {
        const auto remote_files = std::accumulate(remote_storage.begin(), remote_storage.end(),
            QSet<QString>(),
            [&] (QSet<QString>& result, const std::map<QString, remote_res_t>::value_type& pair) {
            if (!pair.second.dir) {
                result << pair.first;
            }
            return result;
            });
        
        const auto local_files = std::accumulate(local_storage.begin(), local_storage.end(),
            QSet<QString>(),
            [&] (QSet<QString>& result, const std::map<QString, QFileInfo>::value_type& pair) {
            if (!pair.second.isDir()) {
                result << pair.first;
            }
            return result;
            });
        
        const auto db_files = std::accumulate(db_storage.begin(), db_storage.end(),
            QSet<QString>(),
            [&] (QSet<QString>& result, const std::map<QString, database::entry_t>::value_type& pair) {
            if (!pair.second.dir) {
                result << pair.first;
                qDebug() << pair.first << pair.second.dir;
            }
            return result;
            });


        qDebug() << "- files -";
        blank_actions << process(db_files, local_files, remote_files, session);
    }
    
    
    {
      const auto remote_dirs = std::accumulate(remote_storage.begin(), remote_storage.end(),
        QSet<QString>(),
        [&] (QSet<QString>& result, const std::map<QString, remote_res_t>::value_type& pair) {
          if (!pair.first.isEmpty() && pair.second.dir) {
            result << pair.first;
          }
          return result;
        });
      
      const auto local_dirs = std::accumulate(local_storage.begin(), local_storage.end(),
        QSet<QString>(),
        [&] (QSet<QString>& result, const std::map<QString, QFileInfo>::value_type& pair) {
          if (pair.second.isDir()) {
            result << pair.first;
          }
          return result;
        });

      const auto db_dirs = std::accumulate(db_storage.begin(), db_storage.end(),
        QSet<QString>(),
        [&] (QSet<QString>& result, const std::map<QString, database::entry_t>::value_type& pair) {
          if (pair.second.dir) {
            result << pair.first;
          }
          return result;
        });
      
      qDebug() << "- dirs -";
      blank_actions << process(db_dirs, local_dirs, remote_dirs, session, lf);
    }
    

    
    qDebug() << "Update results:" << blank_actions;
    
    return blank_actions;
}

QList<action_t> updater_t::process(QSet< QString > db, QSet< QString > local, QSet< QString > remote, session_t& s, QString folder)
{
    const snapshot_data snap(db, local, remote);
    
    qDebug() << Q_FUNC_INFO << folder;
    qDebug() << "-- Local:" << local;
    qDebug() << "-- Remote:" << remote;
    qDebug() << "-- DB:" << db;
    
    QList<action_t> actions;
    
    if (folder.isNull()) {
      Q_FOREACH(const auto entry, snap.to_upload) {
        actions << action_t(
          action_t::upload,
          p_->db->key(folder + "/" + entry),
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_download) {
        actions << action_t(
          action_t::download,
          p_->db->key(folder + "/" + entry),
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_compare) {
        actions << action_t(
          action_t::compare,
          p_->db->key(folder + "/" + entry),
          stat_t(), stat_t()
        );
      }
    }
    else {
      Q_FOREACH(const auto entry, snap.to_upload) {
        actions << action_t(
          action_t::upload_dir,
          p_->db->key(entry),
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_download) {
        actions << action_t(
          action_t::download_dir,
          p_->db->key(entry),
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_compare) {
        if (folder == entry) continue;
        
        const QString localdir = p_->db->key(folder + "/" + entry);
        const QString remotedir = p_->db->key(folder + "/" + entry);
        actions << update(s, localdir, remotedir);
      }
    }

    Q_FOREACH(const auto entry, snap.to_forgot) {
      actions << action_t(
        action_t::forgot,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_remove_local) {
      actions << action_t(
        action_t::remove_local,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_remove_remote) {
      actions << action_t(
        action_t::remove_remote,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
    
    qDebug() << "Processed:" << actions;
    
    return actions;
}

QList<action_t> updater_t::fill(QList<action_t> actions) const
{
  for (auto it = actions.begin(); it != actions.end(); ++it) {
    qDebug() << action_t::type_text(it->type);
    switch(it->type) {
      case action_t::upload:
        it->local = stat_t(p_->local_cache.at(it->key)); break;
      case action_t::download:
        it->remote = stat_t(p_->remote_cache.at(it->key)); 
        break;
      case action_t::compare:
//             qDebug() << QMap<QString, QFileInfo>(p_->local_storage).keys();
        it->local  = stat_t(p_->local_cache.at(it->key));
        it->remote = stat_t(p_->remote_cache.at(it->key));
        it->type = compare(*it);
         break;
      case action_t::forgot: break;
      case action_t::remove_local:
        it->local  = stat_t(p_->local_cache.at(it->key)); break;
      case action_t::remove_remote: 
        it->remote  = stat_t(p_->remote_cache.at(it->key)); break;
      case action_t::upload_dir:
        it->local = stat_t(p_->local_cache.at(it->key)); break;
      case action_t::download_dir:
        it->remote = stat_t(p_->remote_cache.at(it->key)); break;
      default:
        Q_ASSERT(!"Unhandled action type in blank_actions");
    }
  }
  return actions;
}

action_t::type_e updater_t::compare(const action_t& action) const {
  qDebug() << Q_FUNC_INFO;
  qDebug() << "comparing :" << action.key;
  
  const database::entry_t db_entry = p_->db->get(action.key);
  
  const stat_t lstat = action.local;
  const stat_t rstat = action.remote;
  const stat_t dlstat = db_entry.local;
  const stat_t drstat = db_entry.remote;
  
  action_t::TypeMask mask = 0;
  
  if (dlstat.mtime != lstat.mtime) {
      mask |= action_t::local_changed;
      qDebug() << " -> local time changed:" << QDateTime::fromTime_t(dlstat.mtime) << " -> " << QDateTime::fromTime_t(lstat.mtime);
  }
  
  if (dlstat.size != lstat.size) {
      mask |= action_t::local_changed;
      qDebug() << " -> local size changed:" << dlstat.size << " -> " << lstat.size;
  }
  
  if (dlstat.perms != lstat.perms) {
      mask |= action_t::local_changed;
      qDebug() << " -> local permissions changed:" << dlstat.perms << " -> " << lstat.perms;
  }
  
  
  if (drstat.mtime != rstat.mtime) {
      mask |= action_t::remote_changed;
      qDebug() << " -> remote time changed:" << QDateTime::fromTime_t(drstat.mtime) << " -> " << QDateTime::fromTime_t(rstat.mtime);
  }
  
  if (drstat.size != rstat.size) {
      mask |= action_t::remote_changed;
      qDebug() << " -> remote size changed:" << drstat.size << " -> " << rstat.size;
  }
  
  if (drstat.perms != drstat.perms) {
      mask |= action_t::remote_changed;
      qDebug() << " -> remote permissions changed:" << drstat.perms << " -> " << drstat.perms;
  }
  

  if (mask == action_t::local_changed) {
      qDebug() << " -> just upload";
      return action_t::local_changed;
  }
  else if (mask == action_t::remote_changed) {
      qDebug() << " ** -> just download";
      return action_t::remote_changed;
  }
  else if ((mask & action_t::local_changed) && (mask & action_t::remote_changed)) {
      qDebug() << " ** -> CONFLICT";
      return action_t::conflict;
  }
  
  qDebug() << "** -> UNCHANGED";
  return action_t::unchanged;
}


