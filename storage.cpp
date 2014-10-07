
#include <QDir>
#include <QDebug>

#include "types.h"
#include "storage.h"

const QString storage_t::tmpsuffix = ".davqt-tmp";


struct storage_t::Pimpl {
  //Pimpl(const QString& r, QString p, bool k) : root(k ? r + QDir::separator() + p : r), path(p.replace(QRegExp("^[/]*"), "")), keep(k) {}
  Pimpl(const QString& r, QString p, bool k) : root(r), path(p.replace(QRegExp("^[/]*"), "")), keep(k) {}
  
  const QDir root;
  const QString path;
  const bool keep;
};


storage_t::storage_t(const QString& root, const QString& path, bool keep_structure)
  : p_(new Pimpl(root, path, keep_structure))
{
  if (!p_->root.exists()) {
    if (!p_->root.mkpath(".")) {
      throw qt_exception_t(QString("Can't use root dir [%1]").arg(p_->root.absolutePath()));
    }
  }
  
//   if (!p_->path_dir.exists()) {
//     if (!p_->path_dir.mkpath(".")) {
//       throw qt_exception_t(QString("Can't use path dir [%1]").arg(p_->path_dir.absolutePath()));
//     }
//   }
//   
//   p_->path = p_->root.relativeFilePath(p_->path_dir.absolutePath());

  qDebug() << QString("Storage created at [%1]/[%2]").arg(p_->root.absolutePath()).arg(p_->path);
}

storage_t::~storage_t()
{
}

QString storage_t::root() const
{
  return p_->root.absolutePath();
}

QString storage_t::path() const
{
  return p_->path;
}

QString storage_t::prefix() const
{
  if (p_->keep) {
    return root() + QDir::separator() + path();
  }
  else {
    return root();
  }  
}


QString storage_t::key(QString path) const
{
  if (p_->keep) {
    return QString(this->path() + QDir::separator() +  file_path(path)).replace(QRegExp("/$"), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
  }
  else {
    return QString(file_path(path)).replace(QRegExp("/$"), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
  }
}


bool storage_t::keep_structure() const
{
  return p_->keep;
}

QFileInfo storage_t::info(QString raw_key) const
{
  const QString key = raw_key.replace(QRegExp("^[/]*" + path()), "");
  return QFileInfo(prefix() + QDir::separator() + key);
}

QString storage_t::file(const QString& key) const
{
  return info(key).fileName();
}

QString storage_t::folder(const QString& key) const
{
  QString dirname = info(key).dir().absolutePath() + "/";
  return dirname.replace(prefix(), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::file_path(const QString& key) const
{
  QString fullpath = info(key).absoluteFilePath();
  return fullpath.replace(prefix(), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::absolute_file_path(const QString& key) const
{
  return info(key).absoluteFilePath();
}

QString storage_t::remote_file_path(const QString& key) const
{
  return QString("/" + p_->path + "/" + file_path(key)).replace("//", "/");
}

QFileInfoList storage_t::entries(QString raw_key) const
{
  const QString key = raw_key.replace(QRegExp("^[/]*" + path()), "");
  qDebug() << "Storage::entries: " << key;
  QDir dir(absolute_file_path(key));
  qDebug() << "mkapth:" << dir.absolutePath();
  dir.mkpath(".");
  if (!dir.exists()) throw qt_exception_t("Folder " + key + " does not exists");
  
  QFileInfoList result = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
  result.erase(
    std::remove_if( std::begin(result), std::end(result), [] (const QFileInfo& info) {
      return info.fileName().endsWith(storage_t::tmpsuffix);
    }),
    std::end(result)
  ); 
  
  return result;
}


