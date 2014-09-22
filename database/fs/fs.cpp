
#include <QDebug>
#include <QDirIterator>
#include <QMutex>
#include <QMutexLocker>

#include "tools.h"
#include "fs.h"

namespace {
  QDebug debug() {
    return qDebug() << "[DB::FS]:";
  }
}

struct database::fs_t::Pimpl {
  QString dbpath;
  QMutex mx;  
};

database::fs_t::fs_t(const storage_t& s, const QString& dbpath)
  : database_t(s)
  , p_(new Pimpl)
{
  
  QDir dir(dbpath);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      throw qt_exception_t(QString("Can't use db path [%1]").arg(dir.canonicalPath()));
    }
  }
  p_->dbpath = dir.canonicalPath();

  debug() << "loading db:" << p_->dbpath;
}

database::fs_t::~fs_t()
{
}

void database::fs_t::put(const QString& filepath, const db_entry_t& entry)
{
  debug() << "save entry:" << filepath;
  
  QSettings file(item(filepath), QSettings::IniFormat);
  file.clear();
  
  debug() << "name" << file.fileName();
  
  QMutexLocker locker(&p_->mx);    
        
  file.setValue("local/path",  storage().info(filepath).filePath());
  file.setValue("local/mtime", entry.local.mtime);
  file.setValue("local/perms", qint32(entry.local.perms));
  file.setValue("local/size",  entry.local.size);
  
  file.setValue("remote/path",  entry.remote.absolutepath);
  file.setValue("remote/mtime", entry.remote.mtime);
  file.setValue("remote/perms", qint32(entry.remote.perms));
  file.setValue("remote/size",  entry.remote.size);
  
  file.setValue("is_dir",      entry.dir);
  file.setValue("is_bad",      entry.bad);
  
  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't put entry to db [%1]").arg(file.fileName()));
  }
}

db_entry_t database::fs_t::get(const QString& filepath)
{
  debug() << "load entry:" << filepath;
  
  QSettings file(item(filepath), QSettings::IniFormat);
  
  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't get entry from db [%1]").arg(file.fileName()));
  }
  
  return db_entry_t(
      storage().root(),
      storage().folder(filepath),
      storage().file(filepath),
      stat_t(QString(), filepath, 0, 0, -1),
      stat_t(QString(), QString(), 0, 0, -1),
      false
  );

  return db_entry_t();
  
}


void database::fs_t::remove(const QString& absolutefilepath)
{

}

QStringList database::fs_t::folders(QString folder) const
{

}


QStringList database::fs_t::entries(QString folder) const
{

}


QString database::fs_t::item(const QString& path) const
{
  return p_->dbpath + "/" + path;
}



