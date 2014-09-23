
#include <QDebug>
#include <QDirIterator>

#include "tools.h"
#include "fs.h"

namespace {
  QDebug debug() {
    return qDebug() << "[DB::FS]:";
  }
}

struct database::fs_t::Pimpl {
  QDir db;
};

database::fs_t::fs_t(const storage_t& s, const QString& dbpath)
  : database_t(s)
  , p_(new Pimpl)
{
  
  p_->db = QDir(dbpath);
  if (!p_->db.exists()) {
    if (!p_->db.mkpath(".")) {
      throw qt_exception_t(QString("Can't use db path [%1]").arg(p_->db.absolutePath()));
    }
  }
  
  debug() << "loading db:" << p_->db.canonicalPath();
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
  
  Q_FOREACH(const auto& pair, entry.dump().toStdMap()) {
    QVariantMap sub = pair.second.value<QVariantMap>();
    
    if (sub.isEmpty()) {
      file.setValue(pair.first, pair.second);
    } else {
      Q_FOREACH(const auto& sub_pair, sub.toStdMap()) {
        file.setValue(pair.first + "/" + sub_pair.first, sub_pair.second);
      }
    }
  }

  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't put entry to db [%1]").arg(file.fileName()));
  }
}

database::entry_t database::fs_t::get(const QString& filepath) const
{
  debug() << "load entry:" << filepath;
  
  QSettings file(item(filepath), QSettings::IniFormat);
  
  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't get entry from db [%1]").arg(file.fileName()));
  }
  
  
  const stat_t local(
      file.value("local/mtime").value<qlonglong>()
    , QFile::Permissions(file.value("local/perms").toInt())
    , file.value("local/size").value<qulonglong>()
  );
  
  const stat_t remote(
      file.value("remote/mtime").value<qlonglong>()
    , QFile::Permissions(file.value("remote/perms").toInt())
    , file.value("remote/size").value<qulonglong>()
  );

  return create(
      key(filepath),
      local,
      remote,
      file.value("is_dir").value<bool>()
  );
}


void database::fs_t::remove(const QString& filepath)
{
  debug() << "removing entry:" << filepath;
  QFile::remove(item(filepath));
}

QList<database::entry_t> database::fs_t::entries(QString folder) const
{
  qDebug() << "Entries for " << folder;
  
  QDir dir(item(folder).replace(".davdb", ""));
//   if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
  
  qDebug() << "Entries for 2 " << dir.absolutePath();
  
  QList<entry_t> result;
  
  Q_FOREACH(const auto info, dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden)) {
    result << get(info.filePath());
    qDebug() << "Entries for 3 " << info.filePath();
  }
  
  
  
  return result;
}

// QStringList database::fs_t::folders(QString folder) const
// {
//   QDir dir(item(folder));
//   if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
//   
//   return dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
// }
// 
// QStringList database::fs_t::files(QString folder) const
// {
//   QDir dir(item(folder));
//   if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
//   
//   return dir.entryList(QDir::Files | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
// }


QString database::fs_t::item(QString path) const
{
  return p_->db.absoluteFilePath(key(path)) + ".davdb";
}



namespace {
  
const bool self_test = [] () {
  
  {
    debug() << "Test 1";
    
    storage_t storage("/tmp/davtest", "files");
    database::fs_t fs(storage, "/tmp/davtest/db");
    
    QFile f("/tmp/davtest/tmp");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest/tmp");
    
    fs.put("/tmp", fs.create("/tmp", info, info, false));
    auto entry = fs.get("/tmp");

    Q_ASSERT(entry.key == "tmp");
    
    Q_ASSERT(entry.local.mtime == entry.remote.mtime);
    Q_ASSERT(entry.local.mtime == info.lastModified().toTime_t());
    Q_ASSERT(entry.local.size == info.size());
    Q_ASSERT(entry.local.perms == info.permissions());
  }
  
  {
    debug() << "Test 2";
    
    storage_t storage("/tmp/davtest", "files");
    database::fs_t fs(storage, "/tmp/davtest/db");
    
    QFile f("/tmp/davtest/tmp2");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest/tmp2");
    
    fs.put("/sub1/sub2/file", fs.create("/sub1/sub2/file", info, info, false));
    auto entry = fs.get("/sub1/sub2/file");

    Q_ASSERT(entry.key == "sub1/sub2/file");
    
    Q_ASSERT(entry.local.mtime == entry.remote.mtime);
    Q_ASSERT(entry.local.mtime == info.lastModified().toTime_t());
    Q_ASSERT(entry.local.size == info.size());
    Q_ASSERT(entry.local.perms == info.permissions());
    
//     Q_ASSERT(fs.folders() == QStringList() << "sub1");
//     Q_ASSERT(fs.folders("/sub1") == QStringList() << "sub2");
//     Q_ASSERT(fs.folders("sub1/sub2") == QStringList());
    
//     Q_ASSERT(fs.folders() == QStringList() << "sub1");
//     Q_ASSERT(fs.entries("/sub1") == QStringList() << "sub2");
//     Q_ASSERT(fs.entries("sub1/sub2") == QStringList() << "file");
    
    fs.remove("/sub1/sub2/file");
  }
  
   
  
  return true;
}();

}


