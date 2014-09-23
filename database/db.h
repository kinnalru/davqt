#pragma once

#include <memory>

#include <QSettings>
#include <QDir>
#include <QMutex>

#include "types.h"
/*
struct db_t {

    static const QString prefix;
    static const QString tmpprefix;
    
    static bool skip(const QString& path);
    
    db_t(const QString& dbpath, const QString& localroot);
    
    db_t(const db_t&) = delete;
    db_t& operator=(const db_t&) = delete;
   
    
    void save(const QString& absolutefilepath, const db_entry_t& e = db_entry_t());
    
    void remove(const QString& absolutefilepath);
    
    db_entry_t get_entry(const QString& absolutefilepath) const;
    
    QStringList entries(QString folder) const;
    QStringList folders(QString folder) const;
    
private:
    QString dbfolder(const QString& absolutefilepath) const;
    QString dbfile(const QString& absolutefilepath) const;
    
    QString esacpe(QString path) const;
    QString unesacpe(QString escapedpath) const;
    
    db_entry_t& entry(const QString& absolutefilepath);
    
    inline std::unique_ptr<QSettings> settings() {
        return std::unique_ptr<QSettings>(new QSettings(dbpath_, QSettings::IniFormat));
    }
    
    mutable QMutex mx_;
    QString dbpath_;
    QDir localroot_;
    
    typedef std::map<QString, db_entry_t> FolderDb;
    std::map<QString, FolderDb> db_;
};*/