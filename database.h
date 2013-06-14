#pragma once

#include <memory>

#include <QSettings>
#include <QDir>

#include <QDebug>

#include "types.h"


struct db_t {

    static const QString prefix;
    static const QString tmpprefix;
    
    db_t(const QString& dbpath, const QString& localroot);
   
    QString dbfolder(const QString& absolutefilepath) const;
    QString dbfile(const QString& absolutefilepath) const;
    
    void save(const QString& absolutefilepath, const db_entry_t& e = db_entry_t());
    
    void remove(const QString& absolutefilepath);
    
    db_entry_t& entry(const QString& absolutepath);
    
    QStringList entries(QString folder) const;
    
private:
    QString esacpe(QString path) const;
    QString unesacpe(QString escapedpath) const;
    
    inline std::unique_ptr<QSettings> settings() {
        return std::unique_ptr<QSettings>(new QSettings(dbpath_, QSettings::IniFormat));
    }
    
    QString dbpath_;
    QDir localroot_;
    
    typedef std::map<QString, db_entry_t> FolderDb;
    std::map<QString, FolderDb> db_;
};