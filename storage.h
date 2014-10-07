#pragma once

#include <memory>

#include <QFileInfo>
#include <QString>
#include <QStringList>


///class is thread safe
struct storage_t {
  
  static const QString tmpsuffix;
  
  storage_t(const QString& root, const QString& path, bool keep_structure);
  ~storage_t();


  QString key(QString path) const;
  
  bool keep_structure() const;
  
  /// Full info about file
  QFileInfo info(QString key) const;  
  
  /// Only filename without path
  QString file(const QString& key) const;
  
  /// Only folder without root and filename
  QString folder(const QString& key) const;
  
  /// Full path without root
  QString file_path(const QString& key) const;

  QString absolute_file_path(const QString& key) const;
  
  QString remote_file_path(const QString& key) const;
  
  QFileInfoList entries(QString key = QString()) const;




// private:
  /// Full root path of storage
  QString root() const;
  
  /// Relative path from root to files in storage
  QString path() const;

  /// Full path to files in storage
  QString prefix() const;
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};