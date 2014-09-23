
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

void database::fs_t::put(const QString& absolutefilepath, const database::entry_t& entry)
{
  debug() << "save entry:" << absolutefilepath;
  
  QSettings file(item(absolutefilepath), QSettings::IniFormat);
  file.clear();
  
  debug() << "name" << file.fileName();
  
  QMutexLocker locker(&p_->mx);    
        
  file.setValue("local/mtime", entry.local.mtime);
  file.setValue("local/perms", qint32(entry.local.perms));
  file.setValue("local/size",  entry.local.size);
  
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

database::entry_t database::fs_t::get(const QString& filepath)
{
  debug() << "load entry:" << filepath;
  
  QSettings file(item(filepath), QSettings::IniFormat);
  
  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't get entry from db [%1]").arg(file.fileName()));
  }
  
  
  const stat_t local("", storage().file_path(filepath)
    , file.value("local/mtime").value<qlonglong>()
    , QFile::Permissions(file.value("local/perms").toInt())
    , file.value("local/size").value<qulonglong>()
  );
  
  const stat_t remote("", storage().file_path(filepath)
    , file.value("remote/mtime").value<qlonglong>()
    , QFile::Permissions(file.value("remote/perms").toInt())
    , file.value("remote/size").value<qulonglong>()
  );

  qDebug() << filepath;
  qDebug() << storage().folder(filepath);
  
  return entry_t(
      storage().folder(filepath),
      storage().file(filepath),
      local,
      remote,
      file.value("is_dir").value<bool>()
  );

  return entry_t();
  
}


void database::fs_t::remove(const QString& filepath)
{
  debug() << "removing entry:" << filepath;
  QFile::remove(item(filepath));
}

QStringList database::fs_t::folders(QString folder) const
{
  return QDir(item(folder)).entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst);
}


QStringList database::fs_t::entries(QString folder) const
{
  return QDir(item(folder)).entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst);
}


QString database::fs_t::item(const QString& path) const
{
  return p_->dbpath + "/" + path;
}









namespace {
  
const bool self_test = [] () {
  
  {
    qDebug() << "FS test 1";
    
    storage_t storage("/tmp/davtest", "files");
    database::fs_t fs(storage, "/tmp/davtest/db");
    
    QFile f("/tmp/davtest/tmp");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest/tmp");
    
    fs.put("/tmp", database::entry_t("/", "tmp", info, info, false));
    auto entry = fs.get("/tmp");

    Q_ASSERT(entry.name == "tmp");
    Q_ASSERT(entry.folder == "");
    
    Q_ASSERT(entry.local.mtime == entry.remote.mtime);
    Q_ASSERT(entry.local.mtime == info.lastModified().toTime_t());
    Q_ASSERT(entry.local.size == info.size());
    Q_ASSERT(entry.local.perms == info.permissions());
  }
  
  {
    qDebug() << "FS test 2";
    
    storage_t storage("/tmp/davtest", "files");
    database::fs_t fs(storage, "/tmp/davtest/db");
    
    QFile f("/tmp/davtest/tmp2");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest/tmp2");
    
    fs.put("/sub1/sub2/file", database::entry_t("/", "/sub1/sub2/file", info, info, false));
    auto entry = fs.get("/sub1/sub2/file");

    qDebug() << entry.folder;
    
    Q_ASSERT(entry.name == "file");
    Q_ASSERT(entry.folder == "/sub1/sub2");
    
    Q_ASSERT(entry.local.mtime == entry.remote.mtime);
    Q_ASSERT(entry.local.mtime == info.lastModified().toTime_t());
    Q_ASSERT(entry.local.size == info.size());
    Q_ASSERT(entry.local.perms == info.permissions());
    
    fs.remove("/sub1/sub2/file");
  }
  
   
  
  return true;
}();

}


