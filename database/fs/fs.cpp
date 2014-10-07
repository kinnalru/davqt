
#include <QDebug>
#include <QDirIterator>
#include <QMutex>

#include "tools.h"
#include "fs.h"

namespace {
  QDebug debug() {
    return qDebug() << "[DB::FS]:";
  }
}

struct database::fs_t::Pimpl {
  
  Pimpl(const QString& db) : db(db), mx_(QMutex::Recursive), mx(&mx_) {}
  
  const QDir db;
  QMutex mx_;
  QMutex* mx;
};

database::fs_t::fs_t(const storage_t& s, const QString& dbpath)
    : database_t(s)
    , p_(new Pimpl(dbpath))
{
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

QString database::fs_t::key(QString path) const
{
  return database::database_t::key(path.replace(p_->db.absolutePath(), ""));
}


void database::fs_t::put(const QString& filepath, const database::entry_t& entry)
{
  debug() << "save entry:" << filepath << "...";
  QMutexLocker lock(p_->mx);
  
  
  QSettings file(item(filepath), QSettings::IniFormat);
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

QVariantMap to_map(QSettings& s) {
  QVariantMap result;
  Q_FOREACH(auto key, s.childKeys()) {
    result[key] = s.value(key);
  }
  
  Q_FOREACH(auto group, s.childGroups()) {
    s.beginGroup(group);
    result[group] = to_map(s);
    s.endGroup();
  }
  return result;
}

database::entry_t database::fs_t::get(const QString& filepath) const
{
  QMutexLocker lock(p_->mx);
  
  QSettings file(item(filepath), QSettings::IniFormat);
  
  file.sync();
  if (file.status() != QSettings::NoError) {
    throw qt_exception_t(QString("Can't get entry from db [%1]").arg(file.fileName()));
  }
  
  return create(key(filepath), to_map(file));
}


void database::fs_t::remove(const QString& filepath)
{
  debug() << "removing entry:" << filepath << "..."  ;
  QMutexLocker lock(p_->mx);
  
  const auto entry = get(item(filepath));
  if (entry.dir) {
    QDirIterator it(item(filepath).replace(".davdb", ""), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
      it.next();
      QFile::remove(it.fileInfo().absoluteFilePath());
    }    
  }
  QFile::remove(item(filepath));
}

void database::fs_t::clear()
{
  QDirIterator it(p_->db.absoluteFilePath(""), QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    it.next();
    QFile::remove(it.fileInfo().absoluteFilePath());
  }
}

QList<database::entry_t> database::fs_t::entries(QString folder) const
{
  QMutexLocker lock(p_->mx);  
  QDir dir(item(folder).replace(".davdb", ""));
  QList<entry_t> result;
  
  Q_FOREACH(const auto info, dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden)) {
    qDebug() << "FS:" << info.absoluteFilePath();
    if (info.isDir() || info.fileName() == ".davdb") continue;
    qDebug() << "FS FILE:" << info.absoluteFilePath();
    qDebug() << "FS GET:" << info.filePath().replace(".davdb", "");
    qDebug() << "FS RESULT:" << info.filePath().replace(".davdb", "");
    result << get(info.filePath().replace(".davdb", ""));
  }
  
  return result;
}

QString database::fs_t::item(QString path) const
{
  return p_->db.absoluteFilePath(key(path)) + ".davdb";
}

bool database::fs_t::initialized() const
{
  QSettings file(p_->db.absoluteFilePath("") + ".davdb", QSettings::IniFormat);
  return file.value("localstorage").toString() == storage().root() && file.value("keep_structure").toBool() == storage().keep_structure();
}

bool database::fs_t::set_initialized(bool v) const
{
  QSettings file(p_->db.absoluteFilePath("") + ".davdb", QSettings::IniFormat);
  if (v) {
    file.setValue("localstorage", storage().root());
    file.setValue("keep_structure", storage().keep_structure());
  }
  else {
    file.remove("localstorage");
    file.remove("keep_structure");
  }
  file.sync();
}



