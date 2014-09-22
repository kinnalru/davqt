#pragma once


#include "db.h"

struct database_t {
  
    database_t() {}
    virtual ~database_t() {}
  
    virtual void put(const QString& absolutefilepath, const db_entry_t& entry) = 0;
    virtual db_entry_t get(const QString& absolutefilepath) = 0;
    virtual void remove(const QString& absolutefilepath) = 0;
    
    virtual QStringList entries(QString folder) const = 0;
    virtual QStringList folders(QString folder) const = 0;

    database_t(const database_t&) = delete;
    database_t& operator=(const database_t&) = delete;
   
};