#pragma once

#include <memory>

#include "database/database.h"

namespace database {

struct fs_t: public database_t {
  
  fs_t(const QString& dbpath);
  ~fs_t();

  virtual void put(const QString& absolutefilepath, const db_entry_t& entry);
  virtual db_entry_t get(const QString& absolutefilepath);
  virtual void remove(const QString& absolutefilepath);
  
  virtual QStringList entries(QString folder) const;
  virtual QStringList folders(QString folder) const;

private:
  QFileInfo info(const QString& path) const;
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
    
};
  
  
};