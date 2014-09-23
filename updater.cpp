
#include "tools.h"
#include "updater.h"


struct updater_t::Pimpl {
  database_p db;
  connection_t conn;
  bool stop;
};

updater_t::updater_t(database_p db, const connection_t& connection)
  : QObject(0)
  , p_(new Pimpl)
{
  p_->db = db;
  p_->conn = connection;
  p_->stop = false;
  
  Q_VERIFY(connect(this, SIGNAL(finished()), this, SLOT(deleteLater())));
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
        session.cancell();
        Q_EMIT finished();
        return;
      }
      Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));
    }
    
    session.set_auth(p_->conn.login, p_->conn.password);
    session.set_ssl();
    session.open();
    
    qDebug() << Q_FUNC_INFO << "Updating state...";
    const Actions actions = update(session, p_->db->storage().prefix(), p_->db->storage().path());
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

// struct snapshot_data {
//     
//     snapshot_data(const QList<remote_res_t>& rc, const QFileInfoList& lc, const Filenames& local_entries, const Filenames& local_db, const Filenames& remote_entries)
//         : remote_cache(rc)
//         , local_cache(lc)
//         , local_entries(local_entries)
//         , local_db(local_db)
//         , remote_entries(remote_entries)
//         
//         , local_added(local_entries - local_db)
//         , local_deleted(local_db - local_entries)
//         , local_exists(local_db & local_entries)
//         , remote_added(remote_entries - local_added - local_deleted - local_exists)
//     {}
//     
//     const QList<remote_res_t>& remote_cache;
//     const QFileInfoList& local_cache;
//     
//     const QSet<QString> local_entries;
//     const QSet<QString> local_db;
// 
//     const QSet<QString> local_added;
//     const QSet<QString> local_deleted;
//     const QSet<QString> local_exists;
//     
//     const QSet<QString> remote_entries;
//     const QSet<QString> remote_added;
// };


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
    
    to_upload   = exists_local - exists_remote;
    to_compare  = exists_local & exists_remote;
    to_download = exists_remote - exists_local;
    to_forgot   = removed_from_local & removed_from_remote;
    
    to_remove_local = removed_from_remote & exists_local;
    to_remove_remote = removed_from_local & exists_remote;
    
    
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
  
    const QString lf = p_->db->storage().file_path(l);
  
    qDebug() << Q_FUNC_INFO << "Local:" << lf << " Remote:" << rf;
    
    const QList<remote_res_t> remote_storage  = session.get_resources(rf);
    const QFileInfoList       local_storage   =  p_->db->storage().entries(lf);
    const QList<database::entry_t> db_storage = p_->db->entries(lf);
    qDebug() << Q_FUNC_INFO << "Remote cache size:" << remote_storage.size();

    
    
    
    
    
    Q_FOREACH(auto a, local_storage){
      qDebug() << a.absoluteFilePath();
    }
    
    QSet<QString> remote_files = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, remote_storage) {
        if (resource.dir) continue;
        ret << resource.path;
      }
      return ret;
    }();
    
    qDebug() << remote_files;
  
    QSet<QString> local_files = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, local_storage) {
        if (resource.isDir()) continue;
        ret << p_->db->storage().file_path(resource.absoluteFilePath());
      }
      return ret;
    }();
    
    qDebug() << local_files;
    
    
    QSet<QString> db_files = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, db_storage) {
        if (resource.dir) continue;
        ret << resource.filePath();
      }
      return ret;
    }();
    
    
    qDebug() << db_files;
    
    QList<action_t> actions;
    
    actions << process(db_files, local_files, remote_files, session);
    
    
    
    QSet<QString> remote_folders = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, remote_storage) {
        if (!resource.dir) continue;
        if (resource.path.isEmpty()) continue;
        ret << resource.path;
      }
      return ret;
    }();
    
    qDebug() << remote_folders;
  
    QSet<QString> local_folders = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, local_storage) {
        if (!resource.isDir()) continue;
        ret << p_->db->storage().file_path(resource.absoluteFilePath());
      }
      return ret;
    }();
    
    qDebug() << local_folders;
    
    
    QSet<QString> db_folders = [&]() {
      QSet<QString> ret;
      Q_FOREACH(auto resource, db_storage) {
        if (!resource.dir) continue;
        ret << resource.filePath();
      }
      return ret;
    }();
    
    qDebug() << db_folders;
    
    actions << process(db_folders, local_folders, remote_folders, session, lf);
    
    return actions;
}

QList<action_t> updater_t::process(QSet<QString> db, QSet<QString> local, QSet<QString> remote, session_t& s, QString folder)
{
    const snapshot_data snap(db, local, remote);
    
    QList<action_t> actions;
    
    if (folder.isNull()) {
      Q_FOREACH(const auto entry, snap.to_upload) {
        actions << action_t(
          action_t::upload,
          entry, entry,
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_download) {
        actions << action_t(
          action_t::download,
          entry, entry,
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_compare) {
        actions << action_t(
          action_t::compare,
          entry, entry,
          stat_t(), stat_t()
        );
      }
    }
    else {
      Q_FOREACH(const auto entry, snap.to_upload) {
        actions << action_t(
          action_t::upload_dir,
          entry, entry,
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_download) {
        actions << action_t(
          action_t::download_dir,
          entry, entry,
          stat_t(), stat_t()
        );
      }
      
      Q_FOREACH(const auto entry, snap.to_compare) {
        const QString localdir = folder + "/" + entry;
        const QString remotedir = folder + "/" + entry;      
        actions << update(s, localdir, remotedir);
      }
    }

    Q_FOREACH(const auto entry, snap.to_forgot) {
      actions << action_t(
        action_t::forgot,
        entry, entry,
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_remove_local) {
      actions << action_t(
        action_t::remove_local,
        entry, entry,
        stat_t(), stat_t()
      );
    }
    
    Q_FOREACH(const auto entry, snap.to_remove_remote) {
      actions << action_t(
        action_t::remove_remote,
        entry, entry,
        stat_t(), stat_t()
      );
    }
    
    return actions;
}

