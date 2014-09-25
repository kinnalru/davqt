
#include "tools.h"
#include "updater.h"

struct stop_exception_t : public std::runtime_error {
  stop_exception_t() : std::runtime_error("stop") {}
};

namespace {
  QDebug debug() {
    return qDebug() << " > [UPDATH]: ";
  }
};

struct updater_t::Pimpl {
    database_p db;
    manager_t::connection conn;
    bool stop;
    QStringList folders;
    
    std::map<QString, remote_res_t>      remote_cache;
    std::map<QString, QFileInfo>         local_cache;
    std::map<QString, database::entry_t> db_cache;
};

updater_t::updater_t(database_p db, const manager_t::connection& connection)
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

void updater_t::run()
{
  debug() << "Update thread started";
  Q_EMIT started();
  try {
    session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
  
    if (p_->stop) throw stop_exception_t();
    
    Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));
    
    session.set_auth(p_->conn.login, p_->conn.password);
    session.set_ssl();
    session.open();
    
    p_->remote_cache.clear();
    p_->local_cache.clear();
    p_->db_cache.clear();

    p_->folders << p_->db->storage().path();
    
    while(!p_->folders.isEmpty()) {
      const QString current_folder = p_->folders.takeFirst();
      
      const Actions actions = fill(update(session, current_folder));
      if (p_->stop) throw stop_exception_t();
      Q_EMIT new_actions(actions);
    }
  }
  catch (const stop_exception_t& e) {
    debug() << "Update thread force to stop";
  }
  catch (const std::exception& e) {
    debug() << "Update thread exception:" << e.what();
    Q_EMIT error(e.what());
  }

  debug() << "Update thread finished";
  Q_EMIT finished();
}

void updater_t::stop()
{
  p_->stop = true;
  Q_EMIT stopping();
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

Actions updater_t::update(session_t& session, const QString& folder)
{
    if (p_->stop) throw stop_exception_t();
    
    const QString lf = p_->db->key(folder);
    debug() << Q_FUNC_INFO << "Local:" << lf << " Remote:" << folder;
    
    const auto remotes = session.get_resources(folder);
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
    
    Actions blank_actions;
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
                debug() << pair.first << pair.second.dir;
            }
            return result;
            });


        debug() << "- files -";
        blank_actions << process(db_files, local_files, remote_files, session);
        debug() << "F:" << blank_actions;
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
      
      debug() << "- dirs -";
      blank_actions << process(db_dirs, local_dirs, remote_dirs, session, lf);
      debug() << "FD:" << blank_actions;
    }
    

    
    debug() << "Update results:" << blank_actions;
    
    return blank_actions;
}

Actions updater_t::process(QSet<QString> db, QSet<QString> local, QSet<QString> remote, session_t& s, QString folder)
{
  if (p_->stop) throw stop_exception_t();  
  
  debug() << Q_FUNC_INFO << "Fodler:" << folder;
  
  const snapshot_data snap(db, local, remote);
  Actions blank_actions;
  
  if (folder.isNull()) {
    Q_FOREACH(const auto entry, snap.to_upload) {
      blank_actions << action_t(
        action_t::upload,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_download) {
      blank_actions << action_t(
        action_t::download,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_compare) {
      blank_actions << action_t(
        action_t::compare,
        p_->db->key(folder + "/" + entry),
        stat_t(), stat_t()
      );
    }
  }
  else {
    Q_FOREACH(const auto entry, snap.to_upload) {
      blank_actions << action_t(
        action_t::upload_dir,
        p_->db->key(entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_download) {
      blank_actions << action_t(
        action_t::download_dir,
        p_->db->key(entry),
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_compare) {
      if (folder == entry) continue;
      
      p_->folders << p_->db->key(entry);
    }
  }

  Q_FOREACH(const auto entry, snap.to_forgot) {
    blank_actions << action_t(
      action_t::forgot,
      p_->db->key(entry),
      stat_t(), stat_t()
    );
  }
  
  Q_FOREACH(const auto entry, snap.to_remove_local) {
    blank_actions << action_t(
      action_t::remove_local,
      p_->db->key(entry),
      stat_t(), stat_t()
    );
  }
  
  Q_FOREACH(const auto entry, snap.to_remove_remote) {
    blank_actions << action_t(
      action_t::remove_remote,
      p_->db->key(entry),
      stat_t(), stat_t()
    );
  }
  
  return blank_actions;
}

Actions updater_t::fill(Actions actions) const
{
  for (auto it = actions.begin(); it != actions.end(); ++it) {
    debug() << action_t::type_text(it->type);
    switch(it->type) {
      case action_t::upload:
        it->local = stat_t(p_->local_cache.at(it->key)); break;
      case action_t::download:
        it->remote = stat_t(p_->remote_cache.at(it->key)); 
        break;
      case action_t::compare:
//             qdebug() << QMap<QString, QFileInfo>(p_->local_storage).keys();
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
  debug() << Q_FUNC_INFO;
  debug() << "comparing :" << action.key;
  
  const database::entry_t db_entry = p_->db->get(action.key);
  
  const stat_t lstat = action.local;
  const stat_t rstat = action.remote;
  const stat_t dlstat = db_entry.local;
  const stat_t drstat = db_entry.remote;
   
  action_t::TypeMask mask = 0;
  
  if (dlstat.mtime != lstat.mtime) {
      mask |= action_t::local_changed;
      debug() << " -> local time changed:" << QDateTime::fromTime_t(dlstat.mtime) << " -> " << QDateTime::fromTime_t(lstat.mtime);
  }
  
  if (dlstat.size != lstat.size) {
      mask |= action_t::local_changed;
      debug() << " -> local size changed:" << dlstat.size << " -> " << lstat.size;
  }
  
  if (dlstat.perms != lstat.perms) {
      mask |= action_t::local_changed;
      debug() << " -> local permissions changed:" << dlstat.perms << " -> " << lstat.perms;
  }
  
  
  if (drstat.mtime != rstat.mtime) {
      mask |= action_t::remote_changed;
      debug() << " -> remote time changed:" << QDateTime::fromTime_t(drstat.mtime) << " -> " << QDateTime::fromTime_t(rstat.mtime);
  }
  
  if (drstat.size != rstat.size) {
      mask |= action_t::remote_changed;
      debug() << " -> remote size changed:" << drstat.size << " -> " << rstat.size;
  }
  
  if (drstat.perms != drstat.perms) {
      mask |= action_t::remote_changed;
      debug() << " -> remote permissions changed:" << drstat.perms << " -> " << drstat.perms;
  }
  

  if (mask == action_t::local_changed) {
      debug() << " -> just upload";
      return action_t::local_changed;
  }
  else if (mask == action_t::remote_changed) {
      debug() << " ** -> just download";
      return action_t::remote_changed;
  }
  else if ((mask & action_t::local_changed) && (mask & action_t::remote_changed)) {
      debug() << " ** -> CONFLICT";
      return action_t::conflict;
  }
  
  debug() << "** -> UNCHANGED";
  return action_t::unchanged;
}


