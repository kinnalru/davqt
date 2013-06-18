
#include <QDebug>

#include "database.h"


const QString local_c = "local";
const QString remote_c = "remote";
const QString folder_c = "folder";

const QString db_t::prefix = ".davqt";
const QString db_t::tmpprefix = ".davtmp";

struct group_h {
    QSettings& s_;
    group_h(QSettings& s, const QString& group) : s_(s) {s_.beginGroup(group);}
    ~group_h() {s_.endGroup();}
};

db_t::db_t(const QString& dbpath, const QString& localroot)
    : mx_(QMutex::Recursive)
    , dbpath_(dbpath)
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
                stat_t(s->value(local_c).toMap()),
                stat_t(s->value(remote_c).toMap()),
                s->value(folder_c).toBool()
            );
        }
    }
}

bool db_t::skip(const QString& path)
{
    const QFileInfo info(path);
    return info.fileName() == "." || info.fileName() == ".."
        || info.fileName() == db_t::prefix
        || "." + info.suffix() == db_t::tmpprefix;
}

QString db_t::dbfolder(const QString& absolutefilepath) const
{
    QMutexLocker l(&mx_);
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
    QMutexLocker l(&mx_);
    const QString folder = dbfolder(absolutefilepath);
    const QString file = dbfile(absolutefilepath);

    db_entry_t& e = db_[folder][file];
    e.folder = folder;
    e.name = file;
    return e;
}

db_entry_t db_t::get_entry(const QString& absolutefilepath) const
{
    QMutexLocker l(&mx_);
    const QString folder = dbfolder(absolutefilepath);
    const QString file = dbfile(absolutefilepath);

    auto fit = db_.find(folder);
    if (fit != db_.end()) {
        auto it = fit->second.find(file);
        if (it != fit->second.end()) {
            return it->second;
        }
    }
    
    return db_entry_t(
        localroot_.absolutePath(),
        folder,
        file,
        stat_t(QString(), absolutefilepath, 0, 0, -1),
        stat_t(QString(), QString(), 0, 0, -1),
        false
    );
}


void db_t::save(const QString& absolutefilepath, const db_entry_t& e)
{
    QMutexLocker l(&mx_);    
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

    s->setValue(local_c,  sv.local.dump());
    s->setValue(remote_c, sv.remote.dump());
    s->setValue(folder_c, sv.dir);    
}

void db_t::remove(const QString& absolutefilepath)
{
    QMutexLocker l(&mx_);      
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
    QMutexLocker l(&mx_);      
    if (localroot_.absolutePath() == folder)
        folder = ".";
    else
        folder = localroot_.relativeFilePath(folder);

    QStringList items;
    auto it = db_.find(folder);
    if (it == db_.end()) {
        return QStringList();
    }
    else {
        Q_FOREACH(auto& p, it->second) {
            if (p.second.dir) continue;
            items << p.first;
        }
    }
    return items;
}

QStringList db_t::folders(QString folder) const
{
    QMutexLocker l(&mx_);      
    if (localroot_.absolutePath() == folder)
        folder = ".";
    else
        folder = localroot_.relativeFilePath(folder);

    QStringList items;
    auto it = db_.find(folder);
    if (it == db_.end()) {
        return QStringList();
    }
    else {
        Q_FOREACH(auto& p, it->second) {
            if (!p.second.dir) continue;
            items << p.first;
        }
    }
    return items;
}

