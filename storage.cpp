
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
  }
  
  {
    QDir dir(root + "/" + path);
    if (!dir.exists()) {
      if (!dir.mkpath(".")) {
        throw qt_exception_t(QString("Can't use path dir [%1]").arg(dir.canonicalPath()));
      }
    }
    p_->path = dir.canonicalPath().replace(p_->root, "");
  }

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

QFileInfo storage_t::info(const QString& file) const
{
  return (file.startsWith(p_->root + p_->path))
    ? QFileInfo(file)
    : QFileInfo(p_->root + p_->path + "/" + file);
}

QString storage_t::file(const QString& file) const
{
  return info(file).fileName();
}


QString storage_t::folder(const QString& file) const
{
  return info(file).dir().path();
}


