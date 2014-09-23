#pragma once

#include <memory>

#include "database/database.h"

namespace database {

struct fs_t: public database_t {
  
  fs_t(const storage_t& s, const QString& dbpath);
  ~fs_t();

  virtual void put(const QString& filepath, const entry_t& entry);
  virtual entry_t get(const QString& filepath);
  virtual void remove(const QString& filepath);
  
  virtual QStringList entries(QString folder = QString()) const;
  virtual QStringList folders(QString folder = QString()) const;

private:
  QString item(QString path) const;
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
    
};
  
  
};