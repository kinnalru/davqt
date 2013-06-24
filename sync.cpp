/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <functional>

#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QWaitCondition>
#include <QProcess>
#include <QEventLoop>

#include "sync.h"
#include "handlers.h"
#include "tools.h"

typedef const QSet<QString> Filenames;

QList<action_t> scan_and_compare(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder);

action_t::type_e compare(const db_entry_t& dbentry, const local_res_t& local, const remote_res_t& remote)
{
    qDebug() << "comparing :" << local.absoluteFilePath() << " <-> " << remote.path;
    action_t::TypeMask mask = 0;
    
    if (QDateTime::fromTime_t(dbentry.local.mtime) != local.lastModified()) {
        mask |= action_t::local_changed;
        qDebug() << " -> local time changed:" << QDateTime::fromTime_t(dbentry.local.mtime) << " -> " << local.lastModified();
    }
    
    if (dbentry.local.size != local.size()) {
        mask |= action_t::local_changed;
        qDebug() << " -> local size changed:" << dbentry.local.size << " -> " << local.size();
    }
    
    if (dbentry.local.perms != local.permissions()) {
        mask |= action_t::local_changed;
        qDebug() << " -> local permissions changed:" << dbentry.local.perms << " -> " << local.permissions();
    }
    
    
    if (QDateTime::fromTime_t(dbentry.remote.mtime) != QDateTime::fromTime_t(remote.mtime)) {
        mask |= action_t::remote_changed;
        qDebug() << " -> remote time changed:" << QDateTime::fromTime_t(dbentry.remote.mtime) << " -> " << QDateTime::fromTime_t(remote.mtime);
    }
    
    if (dbentry.remote.size != remote.size) {
        mask |= action_t::remote_changed;
        qDebug() << " -> remote size changed:" << dbentry.remote.size << " -> " << remote.size;
    }
    
    if (dbentry.remote.perms != remote.perms) {
        mask |= action_t::remote_changed;
        qDebug() << " -> remote permissions changed:" << dbentry.remote.perms << " -> " << remote.perms;
    }
    
    if (dbentry.remote.etag != remote.etag) {
        mask |= action_t::remote_changed;
        qDebug() << " -> etag changed:" << dbentry.remote.etag << " -> " << remote.etag;
    }
    
    if (dbentry.bad) {
        mask |= action_t::local_changed;
    }
    
    if (mask == action_t::local_changed) return action_t::local_changed;
    if (mask == action_t::remote_changed) return action_t::remote_changed;
    
    if (mask & action_t::local_changed && mask & action_t::remote_changed) {
        qDebug() << " -> CONFLICT";
        return action_t::conflict;
    }
    
    qDebug() << " -> UNCHANGED";
    return action_t::unchanged;
}


struct snapshot_data {
    
    snapshot_data(const QList<remote_res_t>& rc, const QFileInfoList& lc, const Filenames& local_entries, const Filenames& local_db, const Filenames& remote_entries)
        : remote_cache(rc)
        , local_cache(lc)
        , local_entries(local_entries)
        , local_db(local_db)
        , remote_entries(remote_entries)
        
        , local_added(local_entries - local_db)
        , local_deleted(local_db - local_entries)
        , local_exists(local_db & local_entries)
        , remote_added(remote_entries - local_added - local_deleted - local_exists)
    {}
    
    const QList<remote_res_t>& remote_cache;
    const QFileInfoList& local_cache;
    
    const QSet<QString> local_entries;
    const QSet<QString> local_db;

    const QSet<QString> local_added;
    const QSet<QString> local_deleted;
    const QSet<QString> local_exists;
    
    const QSet<QString> remote_entries;
    const QSet<QString> remote_added;
};

//helper function to find fileinfo by name
auto find_info = [] (const QFileInfoList& cache, const QString& file) {
    return std::find_if(cache.begin(), cache.end(), [&] (const QFileInfo& r) { return r.fileName() == file; });
};

//helper function to find resource by name
auto find_resource = [] (const QList<remote_res_t>& cache, const QString& file) {
    return std::find_if(cache.begin(), cache.end(), [&] (const remote_res_t& r) { return r.name == file; });
};

QList<action_t> handle_files(db_t& localdb, const QString& localfolder, const QString& remotefolder, const snapshot_data& snap) {
    QList<action_t> actions;
    
    Q_FOREACH(const QString& file, snap.local_added) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;

        if (!snap.remote_entries.contains(file)) {
            qDebug() << "file " << file << " must be uploaded to server";
            auto fileinfo = find_info(snap.local_cache, file);
            Q_ASSERT(fileinfo != snap.local_cache.end());
            actions.push_back(action_t(action_t::upload,
                localfile,
                remotefile,
                *fileinfo,
                stat_t()));
        }
        else {
            qDebug() << "added file " << file << " already exists on server - must be compared";
            auto fileinfo = find_info(snap.local_cache, file);
            auto resource = find_resource(snap.remote_cache, file);
            Q_ASSERT(fileinfo != snap.local_cache.end());
            Q_ASSERT(resource != snap.remote_cache.end());
            actions.push_back(action_t(compare(localdb.get_entry(localfile), *fileinfo, *resource),
                localfile,
                remotefile,
                *fileinfo,
                stat_t(*resource)));
        }
    }

    Q_FOREACH(const QString& file, snap.local_deleted) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!snap.remote_entries.contains(file)) {
            qDebug() << "file " << file << " deleted and ON server too";
            actions.push_back(action_t(action_t::both_deleted,
                localfile,
                remotefile,
                stat_t(),
                stat_t()));
        }
        else {
            qDebug() << "file " << file << " localy deleted must be compared with etag on server";
            auto resource = find_resource(snap.remote_cache, file);          
            Q_ASSERT(resource != snap.remote_cache.end());            
            actions.push_back(action_t(action_t::local_deleted,
                localfile,
                remotefile,
                stat_t(),
                *resource));            
        }
    }

    Q_FOREACH(const QString& file, snap.local_exists) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!snap.remote_entries.contains(file)) {
            auto fileinfo = find_info(snap.local_cache, file);
            Q_ASSERT(fileinfo != snap.local_cache.end());            
            qDebug() << "file " << file << " deleted on server must be deleted localy";
            actions.push_back(action_t( stat_t(*fileinfo) == localdb.get_entry(localfile).local ? action_t::remote_deleted : action_t::error,
                localfile,
                remotefile,
                *fileinfo,
                stat_t()));            
        }
        else {
            qDebug() << "file " << file << " exists on server must be compared";
            auto fileinfo = find_info(snap.local_cache, file);
            auto resource = find_resource(snap.remote_cache, file);                      
            Q_ASSERT(fileinfo != snap.local_cache.end());   
            Q_ASSERT(resource != snap.remote_cache.end());     
            actions.push_back(action_t(compare(localdb.get_entry(localfile), *fileinfo, *resource),
                localfile,
                remotefile,
                *fileinfo,
                *resource));  
        } 
    }

    Q_FOREACH(const QString& file, snap.remote_added) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        qDebug() << "unhandler remote file:" << file << " must be downloaded";
        
        auto resource = find_resource(snap.remote_cache, file);           
        Q_ASSERT(resource != snap.remote_cache.end());          
        actions.push_back(action_t(action_t::download,
            localfile,
            remotefile,
            stat_t(),
            *resource));          
    }
    
    return actions;
}

QList<action_t> handle_dirs(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder, const snapshot_data& snap) {
    QList<action_t> actions;
    
    Q_FOREACH(const QString& file, snap.local_added) {
        const QString localdir = localfolder + "/" + file;
        const QString remotedir = remotefolder + "/" + file;
        
        if (!snap.remote_entries.contains(file)) {
            qDebug() << "dir " << file << " must be uploaded to server";
            auto fileinfo = find_info(snap.local_cache, file);
            Q_ASSERT(fileinfo != snap.local_cache.end());
            actions.push_back(action_t(action_t::upload_dir,
                localdir,
                remotedir,
                *fileinfo,
                stat_t()));
        }
        else {
            qDebug() << "dir " << file << " already exists on server - normal recursion";
            try {
                actions << scan_and_compare(localdb, session, localdir, remotedir);
            } catch(const std::exception& e) {
                qCritical() << "ERROR: Can't handle internal dir:" << e.what();
                actions.push_back(action_t(action_t::error,
                    localdir,
                    remotedir,
                    *find_info(snap.local_cache, file),
                    *find_resource(snap.remote_cache, file)));  
            }
        }
    }

    Q_FOREACH(const QString& file, snap.local_deleted) {
        const QString localdir = localfolder + "/" + file;
        const QString remotedir = remotefolder + "/" + file;
        
        if (!snap.remote_entries.contains(file)) {
            qDebug() << "dir " << file << " deleted and ON server too";
            actions.push_back(action_t(action_t::both_deleted,
                localdir,
                remotedir,
                stat_t(),
                stat_t()));
        }
        else {
            qDebug() << "dir " << file << " localy deleted must be compared with etag on server";
            auto resource = find_resource(snap.remote_cache, file);          
            Q_ASSERT(resource != snap.remote_cache.end());            
            actions.push_back(action_t(action_t::local_deleted,
                localdir,
                remotedir,
                stat_t(),
                *resource));            
        }
    }
    
    Q_FOREACH(const QString& file, snap.local_exists) {
        const QString localdir = localfolder + "/" + file;
        const QString remotedir = remotefolder + "/" + file;
        
        if (!snap.remote_entries.contains(file)) {
            auto fileinfo = find_info(snap.local_cache, file);
            Q_ASSERT(fileinfo != snap.local_cache.end());            
            qDebug() << "dir " << file << " deleted on server must be deleted localy";
            actions.push_back(action_t(action_t::remote_deleted,
                localdir,
                remotedir,
                *fileinfo,
                stat_t()));            
        }
        else {
            qDebug() << "dir " << file << " just exists on server - normal recursion";
            try {
                actions << scan_and_compare(localdb, session, localdir, remotedir);
            } catch(const std::exception& e) {
                qCritical() << "ERROR: Can't handle internal dir:" << e.what();
                actions.push_back(action_t(action_t::error,
                    localdir,
                    remotedir,
                    *find_info(snap.local_cache, file),
                    *find_resource(snap.remote_cache, file)));  
            }
        } 
    }
    
    Q_FOREACH(const QString& file, snap.remote_added) {
        const QString localdir = localfolder + "/" + file;
        const QString remotedir = remotefolder + "/" + file;
        qDebug() << "unhandler remote dir:" << file << " must be downloaded";
        
        auto resource = find_resource(snap.remote_cache, file);                      
        Q_ASSERT(resource != snap.remote_cache.end());          
        actions.push_back(action_t(action_t::download_dir,
            localdir,
            remotedir,
            stat_t(),
            *resource));          
    }
    
    return actions;
}

QList<action_t> scan_and_compare(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder)
{
    const QList<remote_res_t> remote_cache = session.get_resources(remotefolder);
    
    const QFileInfoList local_cache = [&] {
        QFileInfoList ret;
        Q_FOREACH(const QFileInfo& info, QDir(localfolder).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
            if (db_t::skip(info.fileName())) continue;
            
            ret << info;
        };    
        return ret;
    }();
    
    enum filter {
        folders, files,
    };
    
    //helper function to obtain only file/filder names from complete LOCAL list
    auto justNames = [] (const QFileInfoList& list, filter f) {
        QSet<QString> names;
        Q_FOREACH(const auto& info, list) {
            if (db_t::skip(info.absoluteFilePath())) continue;
            if (f == files && info.isDir() ) continue;
            if (f == folders && !info.isDir() ) continue;
            
            names << info.fileName();
        }
        return names;
    };
    
     //helper function to obtain only file/filder names from complete REMOTE list
    auto remoteNames = [] (const QList<remote_res_t>& list, filter f) {
        QSet<QString> names;
        Q_FOREACH(const auto& resource, list) {
            if (db_t::skip(resource.name)) continue;
            if (f == files && resource.dir) continue;
            if (f == folders && !resource.dir) continue;
            
            names << resource.name;
        }
        return names;
    };
    
    
    QList<action_t> actions;
    
    {
        // Step 1 - making snapshot in filenames of local/remote 'filesystem'
        const snapshot_data filesnap(
            remote_cache,
            local_cache,
            justNames(local_cache, files),
            QSet<QString>::fromList(localdb.entries(localfolder)),
            remoteNames(remote_cache, files)
        );

        actions << handle_files(localdb, localfolder, remotefolder, filesnap);
    }
    
     
    {
        // Step 2 - making snapshot in directories of local/remote 'filesystem'
        const snapshot_data dirsnap(
            remote_cache,
            local_cache,        
            justNames(local_cache, folders),
            QSet<QString>::fromList(localdb.folders(localfolder)),
            remoteNames(remote_cache, folders)
        );
        
        actions << handle_dirs(localdb, session, localfolder, remotefolder, dirsnap);    
    }
    
    return actions;
}

struct runnable_t : public QRunnable {
    template <typename Function>
    runnable_t(Function func) : func_(func) {};
    virtual ~runnable_t() {}
    
    virtual void run () {func_();}
    std::function<void ()> func_;
};

template <typename Function>
void run_in_thread(QThread* th, Function func) {
    QTimer* timer = new QTimer(0);
    timer->setInterval(0);
    timer->setSingleShot(true);
    QObject::connect(th, SIGNAL(started()), timer, SLOT(start()));
    
    ::connect(timer, SIGNAL(timeout()), [th, timer, func] {
        timer->deleteLater();
        func();
    });

    timer->moveToThread(th);
}

struct thread_manager_t::Pimpl {
    
    Pimpl(thread_manager_t::connection conn, const QString& lf, const QString& rf)
        : conn(conn), lf(lf), rf(rf), abort(false), stop(false), updating(false)
        , localdb(lf + "/" + db_t::prefix + "/db", lf)
    {}
    
    const connection conn;
    const QString lf;
    const QString rf;
    db_t localdb;    
    
    QList<QThread*> threads;
    volatile bool stop;
    volatile bool abort;
    
    QMutex mx;
    QWaitCondition needupdate;
    QWaitCondition dataexists;
    bool updating;
    Actions todo;
    Actions in_progress;
};

thread_manager_t::thread_manager_t(QObject* parent, connection conn, const QString& localfolder, const QString& remotefolder)
    : QObject(parent)
    , p_(new Pimpl(conn, localfolder, remotefolder))
{
    static int r1 = qRegisterMetaType<action_t>("action_t");
    static int r2 = qRegisterMetaType<QList<action_t>>("QList<action_t>");    
    static int r3 = qRegisterMetaType<Actions>("Actions");    
    
    {
        QThread* th = new QThread(this);
        QTimer* timer = new QTimer(0);
        timer->setInterval(0);
        timer->setSingleShot(true);
        Q_VERIFY(connect(timer, SIGNAL(timeout()), this, SLOT(update_thread()), Qt::DirectConnection));
        Q_VERIFY(connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater())));          
        Q_VERIFY(connect(th, SIGNAL(started()), timer, SLOT(start()))); 
        timer->moveToThread(th);
        p_->threads << th;
        th->start();
    }

    for (int i = 0; i < QThread::idealThreadCount(); ++i) {
        QThread* th = new QThread(this);
        QTimer* timer = new QTimer(0);
        timer->setInterval(0);
        timer->setSingleShot(true);
        Q_VERIFY(connect(timer, SIGNAL(timeout()), this, SLOT(sync_thread()), Qt::DirectConnection));
        Q_VERIFY(connect(timer, SIGNAL(timeout()), timer, SLOT(deleteLater())));          
        Q_VERIFY(connect(th, SIGNAL(started()), timer, SLOT(start()))); 
        timer->moveToThread(th);
        p_->threads << th;
        th->start();
    }
}

thread_manager_t::~thread_manager_t()
{
    {
        QMutexLocker locker(&p_->mx);
        p_->todo.clear();
        p_->stop = true;
        p_->abort = true;
        Q_EMIT need_stop();
        p_->dataexists.wakeAll();
        p_->needupdate.wakeAll();
    }
    
    Q_FOREACH(QThread* th, p_->threads) {
        th->wait(1000);
    }
    Q_FOREACH(QThread* th, p_->threads) {
        th->quit();
    }
    Q_FOREACH(QThread* th, p_->threads) {
        if (th->isRunning()) {
            th->wait(1000);                       
            th->terminate();
        }
        th->wait(1000);
    }
}

bool thread_manager_t::is_busy()
{
    return !p_->todo.isEmpty() || !p_->in_progress.isEmpty() || p_->updating;
}

void thread_manager_t::update_status()
{
    QMutexLocker locker(&p_->mx);    
    Q_ASSERT(p_->todo.isEmpty());
    Q_ASSERT(p_->in_progress.isEmpty());    
    
    p_->needupdate.wakeAll();
}

void thread_manager_t::start(const Actions& act)
{
    QMutexLocker locker(&p_->mx);
    Q_ASSERT(p_->todo.isEmpty());
    Q_ASSERT(p_->in_progress.isEmpty());
    
    p_->todo = act;
    p_->dataexists.wakeAll();
}

void thread_manager_t::stop()
{
    bool wait = false;
    {
        QMutexLocker locker(&p_->mx);
        p_->stop = true;        
        p_->todo.clear();
        wait = is_busy();
        if (wait) {
            Q_EMIT need_stop();
        }
    }
    if (wait) {
        QEventLoop loop;
        Q_VERIFY(connect(this, SIGNAL(sync_finished()), &loop, SLOT(quit())));
        loop.exec();
    }
    p_->stop = false;
}

void thread_manager_t::update_thread()
{
    Q_FOREVER {
        qDebug() << "upd1";
        if (p_->abort) break;
        {
            QMutexLocker locker(&p_->mx);
            Q_ASSERT(p_->in_progress.isEmpty());
            Q_ASSERT(p_->todo.isEmpty());
            
            p_->needupdate.wait(&p_->mx);
            if (p_->abort) break;
            if (p_->stop) continue;
            
            p_->updating = true;
            Q_EMIT sync_started();
        }
        
        try {
            qDebug() << "upd2:" << p_->conn.schema << " " << p_->conn.host;
            session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
            qDebug() << "upd3";
            {
                QMutexLocker locker(&p_->mx);
                if (p_->stop) {
                    continue;
                }
                Q_VERIFY(connect(this, SIGNAL(need_stop()), &session, SLOT(cancell()), Qt::DirectConnection));
            }
            
            qDebug() << "upd4";
            session.set_auth(p_->conn.login, p_->conn.password);
                        qDebug() << "upd5";
            session.set_ssl();
                        qDebug() << "upd6";
            session.open();
                        qDebug() << "upd7";
            
            qDebug() << "upd8: " << p_->rf;
            const Actions actions = scan_and_compare(p_->localdb, session, p_->lf, p_->rf);    
            qDebug() << "upd9";
            {
                QMutexLocker locker(&p_->mx);        
                qDebug() << "upd10";
                if (!p_->stop) {
                    qDebug() << "upd11";
                    Q_EMIT status_updated(actions);
                }
                qDebug() << "upd12";
                Q_ASSERT(p_->in_progress.isEmpty());
                Q_ASSERT(p_->todo.isEmpty());
                p_->updating = false;
            }
        }
        catch (const std::exception& e) {
            QMutexLocker locker(&p_->mx);        
            if (!p_->stop)
                Q_EMIT status_error(e.what());
            
            Q_ASSERT(p_->in_progress.isEmpty());
            Q_ASSERT(p_->todo.isEmpty());
            p_->updating = false;
        }
        
        Q_EMIT sync_finished();
    }
    QThread::currentThread()->quit();    
}

void thread_manager_t::sync_thread()
{
    auto take_action = [this] () {
        action_t c;
        QMutexLocker locker(&p_->mx);
        
        p_->dataexists.wait(&p_->mx);
        if (!p_->todo.isEmpty()) {
            
            if (p_->in_progress.isEmpty()) {
                Q_EMIT sync_started();
            }
            
            c = p_->todo.takeFirst();
            p_->in_progress << c;
        }
        return c;
    };
    
    auto action_completed = [this] (const action_t& action, bool takenext) {
        action_t c;
        QMutexLocker locker(&p_->mx);
        
        Q_VERIFY(p_->in_progress.removeOne(action));
        
        if (!p_->todo.isEmpty()) {
            if (takenext) {
                c = p_->todo.takeFirst();
                p_->in_progress << c;            
            }
        } 
        else if (p_->in_progress.isEmpty()) {
            Q_EMIT sync_finished();
        }
        return c;
    };

    action_t current;
        
    Q_FOREVER {
        if (p_->abort) break;
        current = take_action();
        if (p_->abort) break;
        if (p_->stop) continue;    
        
        if (current.empty()) continue;
        
        try {
            session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
            
            {
                QMutexLocker locker(&p_->mx);
                if (p_->stop) {
                    current = action_t();
                    continue;
                }
                Q_VERIFY(connect(this, SIGNAL(need_stop()), &session, SLOT(cancell()), Qt::DirectConnection));
            }
            
            session.set_auth(p_->conn.login, p_->conn.password);
            session.set_ssl();
            session.open();
        
            progress_adapter_t adapter;
            
            Q_VERIFY(connect(&session, SIGNAL(get_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
            Q_VERIFY(connect(&session, SIGNAL(put_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
            Q_VERIFY(connect(&adapter, SIGNAL(progress(action_t,qint64,qint64)), this, SIGNAL(progress(action_t,qint64,qint64))));
            
            action_processor_t processor(session, p_->localdb,
                [] (action_processor_t::resolve_ctx& ctx) {
                    return !QProcess::execute(QString("diff"), QStringList() << ctx.local_old << ctx.remote_new);
                },
                [] (action_processor_t::resolve_ctx& ctx) {
                    ctx.result = ctx.local_old + ".merged" + db_t::tmpprefix;
                    return !QProcess::execute(QString("kdiff3"), QStringList() << "-o" << ctx.result << ctx.local_old << ctx.remote_new);
                });
            
            do {
                if (p_->stop) break;
                if (p_->abort) break;
                if (session.is_closed()) break;
                
                adapter.set_action(current);
                
                try {
                    Q_EMIT action_started(current);
                    processor.process(current);
                    if (session.is_closed()) break;
                    Q_EMIT action_success(current);
                }
                catch(const qt_exception_t& e) {
                    Q_EMIT action_error(current, e.message());
                }
                catch(const std::exception& e) {
                    Q_EMIT action_error(current, e.what());
                }
                
                current = action_completed(current, true);
            } while (!current.empty());
        }
        catch (const std::exception& e) {
            Q_EMIT action_error(current, e.what());
            current = action_completed(current, false);
        }
    }
    QThread::currentThread()->quit();
}






/*


thread_pool_t::thread_pool_t(QObject* parent)
    : QThreadPool(parent)
    , busy_(false)
{
    QThread* controler = new QThread(this);
    run_in_thread(controler, [this, controler] {
        Q_FOREVER {
            if (busy_ && this->waitForDone(1000)) {
                busy_ = false;
                Q_EMIT ready();
                sleep(1);
            }
            sleep(1);
        }
    });
    controler->start(); 
}

void thread_pool_t::start(QRunnable* runnable)
{
    busy_ = true;
    Q_EMIT busy();
    QThreadPool::start(runnable);
}



QMutex poolmx;
thread_pool_t* pool_s = NULL;

sync_manager_t::sync_manager_t(QObject* parent, sync_manager_t::connection conn, const QString& localfolder, const QString& remotefolder)
    : QObject(parent)
    , conn_(conn)
    , lf_(localfolder), rf_(remotefolder)
    , localdb_(lf_ + "/" + db_t::prefix + "/db", lf_)
{
    static int r1 = qRegisterMetaType<action_t>("action_t");
    static int r2 = qRegisterMetaType<QList<action_t>>("QList<action_t>");    
    static int r3 = qRegisterMetaType<Actions>("Actions");    
    
    Q_VERIFY(connect(pool(), SIGNAL(busy()), this, SIGNAL(busy())));
    Q_VERIFY(connect(pool(), SIGNAL(ready()), this, SIGNAL(ready())));
}

sync_manager_t::~sync_manager_t()
{}

thread_pool_t* sync_manager_t::pool()
{
    if (!pool_s) {
        QMutexLocker l(&poolmx);
        if (!pool_s) {
            pool_s = new thread_pool_t(0);
            pool_s->setMaxThreadCount(QThread::idealThreadCount());
        }
    }
    return pool_s;
}



void sync_manager_t::update_status()
{
    auto updater =  [this, conn_, &localdb_, lf_, rf_] () mutable {
        try {
            session_t session(0, conn_.schema, conn_.host, conn_.port);
        
            session.set_auth(conn_.login, conn_.password);
            session.set_ssl();
            session.open();
            
            const QList<action_t> actions = scan_and_compare(localdb_, session, lf_, rf_);    
            Q_EMIT status_updated(actions);
        } 
        catch (const std::exception& e) {
            Q_EMIT status_error(e.what());
        }
    };
    
    QDir().mkpath(lf_);
    pool()->start(new runnable_t(updater));
}

QMutex syncmx;

void sync_manager_t::sync(const Actions& act)
{
    if (act.isEmpty()) return;
    
    Q_EMIT sync_started();
    
    //actions are shared between threads - dont forget safe reset
    std::shared_ptr<Actions> actions(new Actions(act));
    auto syncer = [this, conn_, &localdb_, actions] () mutable {
        try {
            
            session_t* session = NULL;
            progress_adapter_t* adapter = NULL;
            action_processor_t* processor = NULL;
            
            auto init_session = [&, this] {
                session = new session_t(0, conn_.schema, conn_.host, conn_.port);
            
                Q_VERIFY(connect(this, SIGNAL(stop_signal()), session, SLOT(cancell()), Qt::DirectConnection));
                
                session->set_auth(conn_.login, conn_.password);
                session->set_ssl();
                session->open();

                adapter = new progress_adapter_t();
                
                Q_VERIFY(connect(session, SIGNAL(get_progress(qint64,qint64)), adapter, SLOT(int_progress(qint64,qint64))));
                Q_VERIFY(connect(session, SIGNAL(put_progress(qint64,qint64)), adapter, SLOT(int_progress(qint64,qint64))));
                Q_VERIFY(connect(adapter, SIGNAL(progress(action_t,qint64,qint64)), this, SIGNAL(progress(action_t,qint64,qint64))));
                
                processor = new action_processor_t(*session, localdb_,
                    [] (action_processor_t::resolve_ctx& ctx) {
                        return !QProcess::execute(QString("diff"), QStringList() << ctx.local_old << ctx.remote_new);
                    },
                    [] (action_processor_t::resolve_ctx& ctx) {
                        ctx.result = ctx.local_old + ".merged" + db_t::tmpprefix;
                        return !QProcess::execute(QString("kdiff3"), QStringList() << "-o" << ctx.result << ctx.local_old << ctx.remote_new);
                    });
            };
            action_t current;
            
            Q_FOREVER {
                if (session && session->is_closed()) break;
                
                {
                    QMutexLocker l(&syncmx);
                    if (actions->isEmpty()) {
                        break;
                    }
                    current = actions->takeFirst();
                }
                
                if (!session) init_session();
                
                adapter->set_action(current);
            
                try {
                    Q_EMIT action_started(current);
                    processor->process(current);
                    Q_EMIT action_success(current);
                }
                catch(const qt_exception_t& e) {
                    Q_EMIT action_error(current, e.message());
                }
                catch(const std::exception& e) {
                    Q_EMIT action_error(current, e.what());
                }
            }
        }
        catch (const std::exception& e) {
            qCritical() << "Can't sync:" << lf_ << rf_;
        }
        
        QMutexLocker l(&syncmx);
        actions.reset();  
    };
    
    Q_VERIFY(::connectOnce(this, SIGNAL(ready()), [this] {Q_EMIT sync_finished();}));
    
    pool()->start(new runnable_t(syncer));
    pool()->start(new runnable_t(syncer));
    pool()->start(new runnable_t(syncer));
}

bool sync_manager_t::is_busy() const
{
    return pool()->activeThreadCount();
}


void sync_manager_t::stop()
{
    qDebug() << "STOP!";
    QEventLoop loop;
    Q_VERIFY(connect(this, SIGNAL(ready()), &loop, SLOT(quit())));
    Q_VERIFY(connect(this, SIGNAL(stop_signal()), &loop, SLOT(quit())));
    QTimer::singleShot(0, this, SIGNAL(stop_signal()));
    
    loop.exec();
    while(is_busy()) {
        QTimer::singleShot(1000, &loop, SLOT(quit()));
        qDebug() << "loop1";
        loop.exec();                        
        qDebug() << "loop2";
    }
}*/







