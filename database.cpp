
#include <QDebug>

#include "database.h"


const QString etag_c = "etag";
const QString local_mtime_c = "local_mtime";
const QString remote_mtime_c = "remote_mtime";
const QString size_c = "size";

const QString db_t::prefix = ".davqt";
const QString db_t::tmpprefix = ".davtmp";

struct group_h {
    QSettings& s_;
    group_h(QSettings& s, const QString& group) : s_(s) {s_.beginGroup(group);}
    ~group_h() {s_.endGroup();}
};

db_t::db_t(const QString& dbpath, const QString& localroot)
    : dbpath_(dbpath)
    , localroot_(localroot)
{
    auto s = settings();
    
    qDebug() << "DB: loading files:" << s->childGroups();
    
    Q_FOREACH(const QString& escaped_folder, s->childGroups()) {
        const QString folder = unesacpe(escaped_folder);
        auto& dbfolder = db_[folder];
        
        group_h g1(*s, escaped_folder);
        Q_FOREACH(const QString& file, s->childGroups()) {
            group_h g2(*s, file);
            
            dbfolder[file] = db_entry_t(
                localroot_.absolutePath(),
                folder,
                file,
                s->value(etag_c).toString(),
                s->value(local_mtime_c).toLongLong(),
                s->value(remote_mtime_c).toLongLong(),
                s->value(size_c).toULongLong()
            );
        }
    }
}

QString db_t::dbfolder(const QString& absolutefilepath) const
{
    const QFileInfo relative = QFileInfo(localroot_.relativeFilePath(absolutefilepath));
    return relative.path();
}

QString db_t::dbfile(const QString& absolutefilepath) const
{
    const QFileInfo relative = QFileInfo(localroot_.relativeFilePath(absolutefilepath));
    return relative.fileName();
}

QString db_t::esacpe(QString path) const
{
    return path.replace("/", "===");
}

QString db_t::unesacpe(QString escapedpath) const
{
    return escapedpath.replace("===", "/"); 
}

db_entry_t& db_t::entry(const QString& absolutefilepath)
{
    const QString folder = dbfolder(absolutefilepath);
    const QString file = dbfile(absolutefilepath);

    db_entry_t& e = db_[folder][file];
    e.folder = folder;
    e.name = file;
    return e;
}

void db_t::save(const QString& absolutefilepath, const db_entry_t& e)
{
    auto s = settings();
    
    const QString folder = dbfolder(absolutefilepath);
    const QString file = dbfile(absolutefilepath);
    const QString escaped_folder = esacpe(folder);

    db_entry_t& sv = entry(absolutefilepath);
    
    if (!e.empty()) {
        Q_ASSERT(e.folder == folder);
        Q_ASSERT(e.name == file);
        sv = e;
    }
    
    qDebug() << "DB: save file:" << folder << " name:" << file  << " raw:" << absolutefilepath;
     
    group_h g1(*s, escaped_folder);
    group_h g2(*s, sv.name);

    s->setValue(etag_c, sv.stat.etag);
    s->setValue(local_mtime_c, qlonglong(sv.stat.local_mtime));
    s->setValue(remote_mtime_c, qlonglong(sv.stat.remote_mtime));
    s->setValue(size_c, qulonglong(sv.stat.size));
}

void db_t::remove(const QString& absolutefilepath)
{
    auto s = settings();

    const QString folder = dbfolder(absolutefilepath);
    const QString file = dbfile(absolutefilepath);
    const QString escaped_folder = esacpe(folder);

    db_entry_t& sv = entry(absolutefilepath);
    
    qDebug() << "DB: DELETE file:" << folder << " name:" << file  << " raw:" << absolutefilepath;
    
    group_h g1(*s, escaped_folder);
    s->remove(sv.name);
    
    db_[folder].erase(file);
}

QStringList db_t::entries(QString folder) const
{
    if (localroot_.absolutePath() == folder)
        folder = ".";
    else
        folder = localroot_.relativeFilePath(folder);

    auto it = db_.find(folder);
    return (it == db_.end())
        ? QStringList()
        : QMap<QString, db_entry_t>(it->second).keys();
}
