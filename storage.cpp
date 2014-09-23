
#include <QDir>
#include <QDebug>

#include "types.h"
#include "storage.h"


struct storage_t::Pimpl {
  QString root;
  QString path;
};


storage_t::storage_t(const QString& root, const QString& path)
  : p_(new Pimpl)
{
  {
    QDir dir(root);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        throw qt_exception_t(QString("Can't use root dir [%1]").arg(root));
      }
    }
    p_->root = dir.canonicalPath();
    
    
    p_->path = dir.relativeFilePath(QString(path).replace(QRegExp("^[/]*"), ""));
    
    if (!p_->path.isEmpty() && !dir.exists(p_->path)) {
      if (!p_->path.isEmpty() &&!dir.mkpath(p_->path)) {
        throw qt_exception_t(QString("Can't use path dir [%1]").arg(root + "/" + path));
      }
    }
  }
  
//   {
//     QDir dir(root + "/" + path);
//     if (!dir.exists()) {
//       if (!dir.mkpath(".")) {
//         throw qt_exception_t(QString("Can't use path dir [%1]").arg(dir.canonicalPath()));
//       }
//     }
//     p_->path = dir.canonicalPath().replace(p_->root, "");
//   }

  qDebug() << QString("Storage created at [%1]/[%2]").arg(p_->root).arg(p_->path);
}

storage_t::~storage_t()
{
}

const QString& storage_t::root() const
{
  return p_->root;
}

const QString& storage_t::path() const
{
  return p_->path;
}

QString storage_t::prefix() const
{
  return QDir(p_->root).absoluteFilePath(p_->path);
}


QFileInfo storage_t::info(const QString& file) const
{
//   QFileInfo
  return (file.startsWith(prefix()))
    ? QFileInfo(file)
    : QFileInfo(prefix() + "/" + file);
}

QString storage_t::file(const QString& file) const
{
  return info(file).fileName();
}


QString storage_t::folder(const QString& file) const
{
  return info(file).dir().path().replace(prefix(), "").replace("//", "/");
}

QString storage_t::file_path(const QString& file) const
{
  return folder(file) + "/" + this->file(file).replace("//", "/");
}









namespace {
  
const bool self_test = [] () {
  
  {
    qDebug() << "Storage test 1";
    storage_t storage("/tmp/davtest", "files");
    
    Q_ASSERT(storage.root() == "/tmp/davtest");
    Q_ASSERT(storage.path() == "files");
    Q_ASSERT(storage.prefix() == "/tmp/davtest/files");
    
    Q_ASSERT(storage.folder("folder1/folder2//fodler3/file") == "/folder1/folder2/fodler3");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "/folder1/folder2/fodler3");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
  }
  
  {
    qDebug() << "Storage test 2";
    storage_t storage("/tmp/davtest", "");
    
    Q_ASSERT(storage.root() == "/tmp/davtest");
    Q_ASSERT(storage.path() == "");
    qDebug() << storage.prefix();
    Q_ASSERT(storage.prefix() == "/tmp/davtest");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/file") == "file");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
    
    Q_ASSERT(storage.folder("folder1/folder2/fodler3/") == "/folder1/folder2/fodler3");
    Q_ASSERT(storage.file("folder1/folder2/fodler3/") == "");
    Q_ASSERT(storage.file_path("folder1/folder2/fodler3/file") == "/folder1/folder2/fodler3/file");
  }
  
   
  
  return true;
}();

}
