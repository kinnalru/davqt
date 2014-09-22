#pragma once

#include "storage.h"
#include "db.h"

struct database_t {
  
  database_t(const storage_t& s) : storage_(s) {}
  virtual ~database_t() {}

  virtual void put(const QString& absolutefilepath, const db_entry_t& entry) = 0;
  virtual db_entry_t get(const QString& absolutefilepath) = 0;
  virtual void remove(const QString& absolutefilepath) = 0;
  
  virtual QStringList entries(QString folder) const = 0;
  virtual QStringList folders(QString folder) const = 0;

  database_t(const database_t&) = delete;
  database_t& operator=(const database_t&) = delete;
   
protected:
  inline const storage_t& storage() const {return storage_;}
    
private:
  const storage_t& storage_;
};