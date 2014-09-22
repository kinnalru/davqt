
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

database::fs_t::fs_t(const QString& dbpath)
  : p_(new Pimpl)
{
  p_->dbpath = dbpath;
  
  debug() << "loading db:" << p_->dbpath;
  
  Q_VERIFY(QFileInfo(p_->dbpath).isDir());
/*  
  QDirIterator it(p_->dbpath, QDirIterator::Subdirectories);
  
  while(it.hasNext()) {
    it.next();
      
    debug() << it.fileInfo().dir().canonicalPath();
  }*/

}

database::fs_t::~fs_t()
{
}

void database::fs_t::put(const QString& filepath, const db_entry_t& entry)
{
  debug() << "save entry:" << filepath;
  
  QSettings file(info(filepath).canonicalFilePath(), QSettings::IniFormat);
  file.clear();
  
  QMutexLocker locker(&p_->mx);    
        
  file.setValue("local/path",  entry.local.absolutepath);
  file.setValue("local/mtime", entry.local.mtime);
  file.setValue("local/perms", qint32(entry.local.perms));
  file.setValue("local/size",  entry.local.size);
  
  file.setValue("remote/path",  entry.remote.absolutepath);
  file.setValue("remote/mtime", entry.remote.mtime);
  file.setValue("remote/perms", qint32(entry.remote.perms));
  file.setValue("remote/size",  entry.remote.size);
  
  file.setValue("is_dir",      entry.dir);
  file.setValue("is_bad",      entry.bad);
}

db_entry_t database::fs_t::get(const QString& filepath)
{
  debug() << "load entry:" << filepath;
  
  QSettings file(info(filepath).canonicalFilePath(), QSettings::IniFormat);
  
  return db_entry_t(
      "root",
      info(filepath).absolutePath(),
      info(filepath).fileName(),
      stat_t(QString(), info(filepath).absolutePath(), 0, 0, -1),
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


QFileInfo database::fs_t::info(const QString& path) const
{
  return QFileInfo(p_->dbpath + "/" + path);
}



