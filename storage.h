#pragma once

#include <memory>

#include <QFileInfo>
#include <QString>
#include <QStringList>


struct storage_t {
  
  storage_t(const QString& root, const QString& path);
  ~storage_t();
  
  QString root() const;
  QString path() const;
  QString prefix() const;
  
  QFileInfo info(const QString& file) const;
  
  QString file(const QString& file) const;
  QString folder(const QString& file) const;
  QString file_path(const QString& file) const;
  
  QFileInfoList entries(QString folder = QString()) const;
  QFileInfoList folders(QString folder = QString()) const;
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};