
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
  QDir dir(item(folder));
  if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
  
  return dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst);
}


QStringList database::fs_t::entries(QString folder) const
{
  QDir dir(item(folder));
  if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
  
  return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst);
}


QString database::fs_t::item(QString path) const
{
  return p_->db.absoluteFilePath(path.replace(p_->db.absolutePath(), "").replace(QRegExp("^[/]*"), ""));
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
    debug() << "Test 2";
    
    storage_t storage("/tmp/davtest", "files");
    database::fs_t fs(storage, "/tmp/davtest/db");
    
    QFile f("/tmp/davtest/tmp2");
    f.open(QIODevice::ReadWrite | QIODevice::Truncate);
    f.write("12345");
    f.close();
    
    QFileInfo info("/tmp/davtest/tmp2");
    
    fs.put("/sub1/sub2/file", database::entry_t("/", "/sub1/sub2/file", info, info, false));
    auto entry = fs.get("/sub1/sub2/file");

    Q_ASSERT(entry.name == "file");
    Q_ASSERT(entry.folder == "/sub1/sub2");
    
    Q_ASSERT(entry.local.mtime == entry.remote.mtime);
    Q_ASSERT(entry.local.mtime == info.lastModified().toTime_t());
    Q_ASSERT(entry.local.size == info.size());
    Q_ASSERT(entry.local.perms == info.permissions());
    
    Q_ASSERT(fs.folders() == QStringList() << "sub1");
    Q_ASSERT(fs.folders("/sub1") == QStringList() << "sub2");
    Q_ASSERT(fs.folders("sub1/sub2") == QStringList());
    
    Q_ASSERT(fs.folders() == QStringList() << "sub1");
    Q_ASSERT(fs.entries("/sub1") == QStringList() << "sub2");
    Q_ASSERT(fs.entries("sub1/sub2") == QStringList() << "file");
    
    fs.remove("/sub1/sub2/file");
  }
  
   
  
  return true;
}();

}


