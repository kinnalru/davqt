#pragma once

#include <memory>

#include <QFileInfo>
#include <QString>
#include <QStringList>


///class is thread safe
struct storage_t {
  
  static const QString tmpsuffix;
  
  storage_t(const QString& root, const QString& path);
  ~storage_t();
  
  QString root() const;
  QString path() const;
  QString prefix() const;
  
  QFileInfo info(const QString& file) const;
  
  /// Only filename without path
  QString file(const QString& file) const;
  
  /// Only full folder without root and filename
  QString folder(const QString& file) const;
  
  /// Full path without root
  QString file_path(const QString& file) const;
  
  QString absolute_file_path(const QString& file) const;
  
  QString remote_file_path(const QString& file) const;
  
  QFileInfoList entries(QString folder = QString()) const;
  
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};