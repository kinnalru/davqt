#pragma once

#include <memory>

#include "storage.h"

#include "db.h"
#include "entry.h"

namespace database {

struct database_t {
  
  database_t(const storage_t& s) : storage_(s) {}
  virtual ~database_t() {}
  
  virtual QString key(QString path) const;

  virtual void put(const QString& key, const entry_t& entry) = 0;
  virtual entry_t get(const QString& key) const = 0;
  virtual void remove(const QString& key) = 0;
  
  virtual QList<entry_t> entries(QString folder = QString()) const = 0;

  entry_t create(QString path, const UrlInfo& l, const UrlInfo& r, bool dir) const;
  entry_t create(QString path, const QVariantMap& data) const;
  
  database_t(const database_t&) = delete;
  database_t& operator=(const database_t&) = delete;
   
  inline const storage_t& storage() const {return storage_;}
  
  virtual void clear() = 0;
  
  ///Used to detect first initialized datafiles
  virtual bool initialized() const = 0;
  virtual bool set_initialized(bool) const = 0;
    
private:
  const storage_t& storage_;
};


}

typedef std::shared_ptr<database::database_t> database_p;