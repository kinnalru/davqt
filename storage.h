#pragma once

#include <memory>

#include <QFileInfo>
#include <QString>


struct storage_t {
  
  storage_t(const QString& root, const QString& path);
  ~storage_t();
  
  const QString& root() const;
  const QString& path() const;
  QString prefix() const;
  
  QFileInfo info(const QString& file) const;
  
  QString file(const QString& file) const;
  QString folder(const QString& file) const;
  QString file_path(const QString& file) const;
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};