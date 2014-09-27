
#include <sys/stat.h> 
#include <fcntl.h> 

#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <QProcess>

#include "manager.h"
#include "handlers.h"

namespace {
  QDebug debug() {
    return qDebug() << "[HANDLERS]:";
  }
}

void handler_t::run()
{
  Q_EMIT started(action);
  try {
    do_check();
    do_request();
  }
  catch (std::exception& e) {
      Q_EMIT error(action, e.what());
  }
  Q_EMIT finished(action);
}

QNetworkReply* handler_t::deliver_and_block(handler_t::synchro_reply_f f) const
{
  QNetworkReply* reply = 0;
  
  QEventLoop loop;
  
  new Package(&manager, [&, this] () {
    reply = f(manager.network());
    if (reply) {
      reply->moveToThread(this->thread());
      connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
      connect(this, SIGNAL(need_stop()), reply, SLOT(abort()));
      reply->setParent(const_cast<QObject*>(static_cast<const QObject*>(this)));
    }
  },
  Qt::BlockingQueuedConnection);

  if (reply) {
    loop.exec();
  }
  
  if (reply && reply->error() != QNetworkReply::NoError) {
    throw qt_exception_t(reply->errorString());
  }
  
  return reply;
}


void upload_handler::do_check() const {
  Q_ASSERT(action.local.ready()); // local contais NEW "fixed" file stat to upload.
  Q_ASSERT(!action.remote.isValid()); // remote MUST not exists - sanity check
}
  
void upload_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  
  const QString local_file = db->storage().absolute_file_path(action.key);
  const QString remote_file = db->storage().remote_file_path(action.key);
  
  QFile file(local_file);
  
  if (!file.open(QIODevice::ReadOnly)) 
    throw qt_exception_t(QString("Can't open file %1: %2").arg(action.key).arg(file.errorString()));
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->put(remote_file, &file);
  });
  file.flush();
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->setPermissions(remote_file, action.local.permissions());
  });

  const UrlInfo remote = deliver([&] (QWebdav* webdav) {
    return webdav->info(remote_file);
  });
  
  Q_ASSERT(!action.local.isDir());
  Q_ASSERT(!remote.isDir());
  
  database::entry_t e = db->create(action.key, action.local, remote, false);
  db->put(e.key, e);
}


void download_handler::do_check() const {
  Q_ASSERT(action.remote.ready());
}

void download_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  
  const QString local_file = db->storage().absolute_file_path(action.key);
  const QString remote_file = db->storage().remote_file_path(action.key);      
  
  const QString tmppath = local_file + storage_t::tmpsuffix;
  
  if (QFileInfo(tmppath).exists())
    throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg("file exists!"));
  
  QFile tmpfile(tmppath);
  if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
    throw qt_exception_t(QString("Can't create file %1: ").arg(tmppath).arg(tmpfile.errorString()));
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->get(remote_file, &tmpfile);
  });
  tmpfile.flush();
  
  tmpfile.setPermissions(QFile::Permissions(action.remote.permissions()));

  const UrlInfo remote = deliver([&] (QWebdav* webdav) {
    return webdav->info(remote_file);
  });
  
  QFile::remove(local_file);
  
  if (!tmpfile.rename(local_file))
    throw qt_exception_t(QString("Can't rename file %1 -> %2: %3").arg(tmppath).arg(local_file).arg(tmpfile.errorString()));   

  const UrlInfo local(local_file);
  
  Q_ASSERT(!action.local.isDir());
  Q_ASSERT(!remote.isDir());
  
  database::entry_t e = db->create(action.key, local, remote, false);
  db->put(e.key, e);
}

void forgot_handler::do_check() const {
  Q_ASSERT(action.local.empty()); // local MUST not exists - sanity check
  Q_ASSERT(action.remote.empty());// remote MUST not exists - sanity check
}

void forgot_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
    
  db->remove(action.key);
}


void local_change_handler::do_check() const {
    Q_ASSERT(action.local.ready());  // local contais NEW "fixed" file stat to upload.
    Q_ASSERT(action.local.ready()); // remote MUST exists - sanity check
}

void local_change_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  
  database::entry_t e = db->get(action.key);
  if (e.local.size() == action.local.size() &&
      e.local.lastModified() == action.local.lastModified() &&
      e.local.permissions() != action.local.permissions())
  {
    const QString remote_file = db->storage().remote_file_path(action.key);      
    
    deliver_and_block([&] (QWebdav* webdav) {
      return webdav->setPermissions(remote_file, action.local.permissions());
    });

    const UrlInfo remote = deliver([&] (QWebdav* webdav) {
      return webdav->info(remote_file);
    });

    Q_ASSERT(!action.local.isDir());
    Q_ASSERT(!remote.isDir());
    
    database::entry_t e = db->create(action.key, action.local, remote, false);
    db->put(e.key, e);
  }
  else {
    upload_handler::do_request();
  }
}

void remote_change_handler::do_check() const {
  Q_ASSERT(action.local.ready()); // local MUST exists - sanity check
  Q_ASSERT(action.remote.ready());// remote MUST exists - sanity check        
}

void conflict_handler::do_check() const {
  Q_ASSERT(action.local.ready());  // local MUST exists - sanity check
  Q_ASSERT(action.remote.ready()); // remote MUST exists - sanity check    
}
  
void conflict_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  
  const QString local_file = db->storage().absolute_file_path(action.key);
  const QString remote_file = db->storage().remote_file_path(action.key);      

  Q_ASSERT(!QFileInfo(local_file).isDir());
  
  const QString tmppath = local_file + storage_t::tmpsuffix;
    
  QFile tmpfile(tmppath);
  if (!tmpfile.open(QIODevice::ReadWrite | QIODevice::Truncate)) 
    throw qt_exception_t(QString("Can't create file %1: %2").arg(tmppath).arg(tmpfile.errorString()));        

    deliver_and_block([&] (QWebdav* webdav) {
      return webdav->get(remote_file, &tmpfile);
    });
    tmpfile.flush();
  
    tmpfile.setPermissions(QFile::Permissions(action.remote.permissions()));
    
    const UrlInfo remote = deliver([&] (QWebdav* webdav) {
      return webdav->info(remote_file);
    });

    Q_ASSERT(!action.local.isDir());
    Q_ASSERT(!remote.isDir());
    
    bool result = false;
    conflict_ctx ctx = {
      action,
      local_file,
      tmppath,
      ""
    };
    
    Q_EMIT compare(ctx, result);
    if (result) {
      //File contents is same
      QFile::remove(tmppath); 
      
      const UrlInfo local(local_file);
      
      deliver_and_block([&] (QWebdav* webdav) {
        return webdav->setPermissions(remote_file, local.permissions());
      });
      
      const UrlInfo remote = deliver([&] (QWebdav* webdav) {
        return webdav->info(remote_file);
      });
      
      Q_ASSERT(!local.isDir());
      Q_ASSERT(!remote.isDir());
      
      database::entry_t e = db->create(action.key, local, remote, false);
      db->put(e.key, e);
    } 
  else {
    //File contents differs
    result = false;
    Q_EMIT merge(ctx, result);
    if (result) {
      //Files merged
      QFile::remove(tmppath); 
      QFile::remove(local_file);
      
      if (!QFile::rename(ctx.result_file, local_file))
        throw qt_exception_t(QString("Can't rename file %1 -> %2").arg(ctx.result_file).arg(local_file));                       

      QFile::setPermissions(local_file, QFile::Permissions(action.local.permissions()));
      const UrlInfo local(local_file);

      database::entry_t e = db->create(action.key, local, remote, false);
      db->put(e.key, e);
      
      Q_ASSERT(!local.isDir());
      Q_ASSERT(!remote.isDir());
    
      const action_t act(action_t::local_changed,
        action.key,
        local,
        remote);
      
      Q_EMIT new_actions(Actions() << act);
    }
    else {
      throw qt_exception_t("Can't resolve conflict!");
    }
  }
  
}

void remove_local_handler::do_check() const {
  Q_ASSERT(action.local.ready());   // local MUST exists - sanity check
  Q_ASSERT(!action.remote.isValid()); // remote MUST not exists - for ETag compare   
}

bool remove_local_handler::remove_dir(const QString & path) const
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

void remove_local_handler::do_request() const {
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
}

void remove_remote_handler::do_check() const {
  Q_ASSERT(!action.local.isValid()); // local MUST not exists - for changes compare 
  Q_ASSERT(action.remote.ready()); // remote MUST exists - sanity
}

void remove_remote_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  const QString remote_file = db->storage().remote_file_path(action.key);
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->remove(remote_file);
  });
  
  db->remove(action.key);
}

void upload_dir_handler::do_check() const {
  Q_ASSERT(!action.local.empty()); // local MUST exists - sanity
  Q_ASSERT(action.remote.empty()); // remote MUST not exists - sanity
}

void upload_dir_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  const QString local_file = db->storage().absolute_file_path(action.key);
  const QString remote_file = db->storage().remote_file_path(action.key);
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->mkcol(remote_file);
  });
  
  deliver_and_block([&] (QWebdav* webdav) {
    return webdav->setPermissions(remote_file, action.local.permissions());
  });
  
  const UrlInfo remote = deliver([&] (QWebdav* webdav) {
    return webdav->info(remote_file);
  });

  Q_ASSERT(action.local.isDir());
  Q_ASSERT(remote.isDir());
  
  database::entry_t e = db->create(action.key, action.local, remote, true);
  db->put(e.key, e);

  Actions result;
  
  Q_FOREACH(const QFileInfo& info, QDir(local_file).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden | QDir::System)) {
    result << action_t(
      (info.isDir() ? action_t::upload_dir : action_t::upload),
      db->key(info.absoluteFilePath()),
      UrlInfo(info),
      UrlInfo()
    );
  }
  
  Q_EMIT new_actions(result);
}

void download_dir_handler::do_check() const {
  Q_ASSERT(!action.local.isValid()); // local MUST not exists - sanity
  Q_ASSERT(action.remote.ready()); // remote MUST exists - sanity
}
  
void download_dir_handler::do_request() const {
  qDebug() << Q_FUNC_INFO;
  
  const QString local_file = db->storage().absolute_file_path(action.key);
  const QString remote_file = db->storage().remote_file_path(action.key);
  
  if (!QFileInfo(local_file).exists() && !QDir().mkdir(local_file))
    throw qt_exception_t(QString("Can't create dir: %1").arg(local_file));

  QFile::setPermissions(local_file, QFile::Permissions(action.remote.permissions()));

  const UrlInfo local(local_file);

  Q_ASSERT(local.isDir());
  Q_ASSERT(action.remote.isDir());
  
  database::entry_t e = db->create(action.key, local, action.remote, true);
  db->put(e.key, e);
  
  Actions result;
  
  const auto resources = deliver([&] (QWebdav* webdav) {
    return webdav->parse(webdav->list(remote_file));
  });
    
  Q_FOREACH(const QWebdavUrlInfo& resource, resources) {
    if (action.key == db->key(resource.name())) continue;
    
    result << action_t(
      (resource.isDir() ? action_t::download_dir : action_t::download),
      db->key(resource.name()),
      UrlInfo(),
      UrlInfo(resource)
    ); 
  }
  
  Q_EMIT new_actions(result);
}


