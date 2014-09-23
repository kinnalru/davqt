
#include <QDir>
#include <QDebug>

#include "types.h"
#include "storage.h"

const QString storage_t::tmpsuffix = ".davqt";


struct storage_t::Pimpl {
  QDir root;
  QDir path;
};


storage_t::storage_t(const QString& root, const QString& path)
  : p_(new Pimpl)
{
  {
    p_->root = QDir(root);
    if (!p_->root.exists()) {
      if (!p_->root.mkpath(".")) {
        throw qt_exception_t(QString("Can't use root dir [%1]").arg(p_->root.absolutePath()));
      }
    }
    
    p_->path = QDir(root + QDir::separator() + path);
    
    if (!p_->path.exists()) {
      if (!p_->path.mkpath(".")) {
        throw qt_exception_t(QString("Can't use path dir [%1]").arg(p_->path.absolutePath()));
      }
    }
  }

  qDebug() << QString("Storage created at [%1]/[%2]").arg(p_->root.absolutePath()).arg(p_->root.relativeFilePath(p_->path.absolutePath()));
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
  qDebug() << "p:" << p_->path.absolutePath();
  return p_->root.relativeFilePath(p_->path.absolutePath());
}

QString storage_t::prefix() const
{
  return p_->path.absolutePath();
}

QFileInfo storage_t::info(const QString& file) const
{
  return (file.startsWith(prefix()))
    ? QFileInfo(file)
    : QFileInfo(prefix() + QDir::separator() + file);
}

QString storage_t::file(const QString& file) const
{
  return info(file).fileName();
}

QString storage_t::folder(const QString& file) const
{
  QString dirname = info(file).dir().absolutePath() + "/";
  return dirname.replace(prefix(), "").replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::file_path(const QString& file) const
{
  return QString(folder(file) + QDir::separator() + this->file(file)).replace(QRegExp("^[/]*"), "").replace("//", "/");
}

QString storage_t::absolute_file_path(const QString& file) const
{
  return QString(p_->root.absolutePath() + QDir::separator() + file_path(file)).replace("//", "/");
}

QString storage_t::remote_file_path(const QString& file) const
{
  return QString("/" + file_path(file)).replace("//", "/");
}


QFileInfoList storage_t::entries(QString folder) const
{
  QDir dir(prefix() + QDir::separator() + folder);
  if (!dir.exists()) throw qt_exception_t("Folder " + folder + " does not exists");
  
  return dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDir::DirsFirst);
}








namespace {
  
const bool self_test = [] () {
  
  {
    qDebug() << "Storage test 1";
    storage_t storage("/tmp/davtest", "files");
    
    Q_ASSERT(storage.root() == "/tmp/davtest");
    Q_ASSERT(storage.path() == "files");
    Q_ASSERT(storage.prefix() == "/tmp/davtest/files");
    
    Q_ASSERT(storage.folder("folder1/folder2//fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
  }
  
  {
    qDebug() << "Storage test 2";
    storage_t storage("/tmp/davtest", "");
    
    Q_ASSERT(storage.root() == "/tmp/davtest");
    Q_ASSERT(storage.path() == "");
    Q_ASSERT(storage.prefix() == "/tmp/davtest");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "folder1/folder2/fodler3/");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "folder1/folder2/fodler3/file");
  }
  
   
  
  return true;
}();

}
