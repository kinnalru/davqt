
#include <iostream>
#include <istream>
#include <ostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <memory>
#include <string.h>
#include <assert.h>
#include <libgen.h>
#include <stdio.h>
#include <fcntl.h> 

#include <QDir>
#include <QDateTime>
#include <QSet>
#include <QDebug>

#include <neon/ne_alloc.h>
#include <neon/ne_auth.h>
#include <neon/ne_basic.h>
#include <neon/ne_dates.h>
#include <neon/ne_locks.h>
#include <neon/ne_props.h>
#include <neon/ne_request.h>
#include <neon/ne_session.h>
#include <neon/ne_socket.h>
#include <neon/ne_string.h>
#include <neon/ne_uri.h>
#include <neon/ne_utils.h>
#include <neon/ne_xml.h>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/concept_check.hpp>

#include "session.h"
#include "sync.h"
#include "handlers.h"


struct dav_props {
    char *path;         /* The unescaped path of the resource. */
    char *name;         /* The name of the file or directory. Only the last
                           component (no path), no slashes. */
    char *etag;         /* The etag string, including quotation characters,
                           but without the mark for weak etags. */
    off_t size;         /* File size in bytes (regular files only). */
    time_t ctime;       /* Creation date. */
    time_t mtime;       /* Date of last modification. */
    int is_dir;         /* Boolean; 1 if a directory. */
    int is_exec;        /* -1 if not specified; 1 is executeable;
                           0 not executeable. */
    dav_props *next;    /* Next in the list. */
};


typedef struct {
    const char *path;           /* The *not* url-encoded path. */
    dav_props *results;         /* Start of the linked list of dav_props. */
} propfind_context;


static void
ls_result(void *userdata, const ne_uri *uri, const ne_prop_result_set *set)
{
    std::vector<std::string>* res = reinterpret_cast<std::vector<std::string>*>(userdata); 

    assert(res);
    assert(uri);
    assert(set);
    
    //FIXME check
    //if (!res || !uri || !uri->path || !set) return;

    res->push_back(uri->path);
    
    return;
}


QList<action_t> handle_dir(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder) {
    
    std::cerr << "hd 1" << std::endl;
    const std::vector<remote_res_t> remote_entries = session.get_resources(remotefolder.toStdString());
    std::cerr << "hd 2" << std::endl;
    QFileInfoList local_entries;
    
    Q_FOREACH(const QFileInfo& info, QDir(localfolder).entryInfoList(QDir::AllEntries | QDir::AllDirs | QDir::Hidden | QDir::System)) {
        if (info.fileName() == "." || info.fileName() == "..") continue;
        if (info.fileName() == db_t::prefix || info.suffix() == db_t::tmpprefix) continue;
            
        local_entries << info;
    };
    
    QList<action_t> actions;
    
    enum recursion {
        dirs, files,
    };
    
    auto justNames = [] (const QFileInfoList& list, recursion r) {
        QSet<QString> names;
        BOOST_FOREACH(const auto& info, list) {
            if (info.fileName() == "." || info.fileName() == "..") continue;
            if (r == files && info.isDir() ) continue;
            if (r == dirs && !info.isDir() ) continue;
            
            std::cerr << "local:" << info.fileName().toStdString() << std::endl;
            names << info.fileName();
        }
        return names;
    };
    
    auto remoteNames = [] (const std::vector<remote_res_t>& list, recursion r) {
        QSet<QString> names;
        BOOST_FOREACH(const auto& resource, list) {
            if (resource.name == "." || resource.name == "..") continue;    
            if (r == files && resource.dir) continue;
            if (r == dirs && !resource.dir) continue;
            
            std::cerr << "remote:" << resource.name << std::endl;
            names << resource.name.c_str();
        }
        return names;
    };
    
    auto find_info = [&local_entries] (const QString& file) {
        return std::find_if(local_entries.begin(), local_entries.end(), [&] (const QFileInfo& r) { return r.fileName() == file; });
    };
    
    auto find_resource = [&remote_entries] (const QString& file) {
        return std::find_if(remote_entries.begin(), remote_entries.end(), [&] (const remote_res_t& r) { return r.name == file.toStdString(); });
    };
    
    const QSet<QString> local_entries_set = justNames(local_entries, files);
    const QSet<QString> local_db_set = QSet<QString>::fromList(localdb.entries(localfolder));

    const QSet<QString> local_added = local_entries_set - local_db_set;
    const QSet<QString> local_deleted = local_db_set - local_entries_set;        
    const QSet<QString> local_exists = local_db_set & local_entries_set;
    
    const QSet<QString> remote_entries_set = remoteNames(remote_entries, files);
    const QSet<QString> remote_unhandled = remote_entries_set - local_added - local_deleted - local_exists;
    
    qDebug() << "local_entries_set:" << QStringList(local_entries_set.toList()).join(";");
    qDebug() << "local_db_set:" << QStringList(local_db_set.toList()).join(";");
    qDebug() << "local_added:" << QStringList(local_added.toList()).join(";");
    qDebug() << "local_deleted:" << QStringList(local_deleted.toList()).join(";");
    qDebug() << "local_exists:" << QStringList(local_exists.toList()).join(";");    
    
    BOOST_FOREACH(const QString& file, local_added) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            std::cerr << "file " << file.toStdString() << " must be uploaded to server" << std::endl;
            actions.push_back(action_t(action_t::upload,
                localdb.entry(localfile),
                localfile,
                remotefile,
                *find_info(file),
                remote_res_t()));
        }
        else {
            std::cerr << "file " << file.toStdString() << " already exists on server - must be compared" << std::endl;
            auto it = find_info(file);
            assert(it != local_entries.end());
            actions.push_back(action_t(compare(localdb.entry(localfile), *it, *find_resource(file)),
                localdb.entry(localfile),
                localfile,
                remotefile,
                *it,
                *find_resource(file)));
        }
    }
    
    BOOST_FOREACH(const QString& file, local_deleted) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            std::cerr << "file " << file.toStdString() << " deleted on server too" << std::endl;
            actions.push_back(action_t(action_t::both_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                QFileInfo(),
                remote_res_t()));
        }
        else {
            std::cerr << "file " << file.toStdString() << " localy deleted must be compared with etag on server" << std::endl;
            actions.push_back(action_t(action_t::local_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                QFileInfo(),
                *find_resource(file)));            
        }
    }
    
    BOOST_FOREACH(const QString& file, local_exists) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        if (!remote_entries_set.contains(file)) {
            std::cerr << "file " << file.toStdString() << " deleted on server must be deleted localy" << std::endl;
            actions.push_back(action_t(action_t::remote_deleted,
                localdb.entry(localfile),
                localfile,
                remotefile,
                *find_info(file),
                remote_res_t()));            
        }
        else {
            std::cerr << "file " << file.toStdString() << " exists on server must be compared" << std::endl;
            auto it = find_info(file);
            assert(it != local_entries.end());
            actions.push_back(action_t(compare(localdb.entry(localfile), *it, *find_resource(file)),
                localdb.entry(localfile),
                localfile,
                remotefile,
                *find_info(file),
                *find_resource(file)));            
        }
    }
    
    BOOST_FOREACH(const QString& file, remote_unhandled) {
        const QString localfile = localfolder + "/" + file;
        const QString remotefile = remotefolder + "/" + file;
        
        std::cerr << "unhandler remote file:" << file.toStdString() << " must be downloaded" << std::endl;
        actions.push_back(action_t(action_t::download,
            localdb.entry(localfile),
            localfile,
            remotefile,
            QFileInfo(),
            *find_resource(file)));          
    }
        
        
    //recursion
    
    QSet<QString> completed;
    
    BOOST_FOREACH(const QString& dir, justNames(local_entries, dirs)) {
        const QString localdir = localfolder + "/" + dir;
        const QString remotedir = remotefolder + "/" + dir;
        
        auto it = std::find_if(remote_entries.begin(), remote_entries.end(), [&] (const remote_res_t& r) { return r.name == dir.toStdString(); });
        
        if (it == remote_entries.end()) {
            std::cerr << "dir " << dir.toStdString() << " must be uploaded to server" << std::endl;
            actions.push_back(action_t(action_t::upload_dir,
                db_entry_t(),
                localdir,
                remotedir,
                *find_info(dir),
                remote_res_t()));              
        }
        else if (it->dir) {
            std::cerr << "dir " << dir.toStdString() << " already exists on server - recursion" << std::endl;
            actions << handle_dir(localdb, session, localdir, remotedir);
        }
        else {
            std::cerr << "dir " << dir.toStdString() << " already exists on server - BUT NOT A DIR" << std::endl;
            actions.push_back(action_t(action_t::error,
                db_entry_t(),
                localdir,
                remotedir,
                *find_info(dir),
                *it));              
        }
        
        completed << dir;
    }  
    
    BOOST_FOREACH(const QString& dir, remoteNames(remote_entries, dirs)) {
        if (completed.contains(dir)) continue;
        
        const QString localdir = localfolder + "/" + dir;
        const QString remotedir = remotefolder + "/" + dir;
        
        auto it = std::find_if(local_entries.begin(), local_entries.end(), [&] (const QFileInfo& r) { return r.fileName() == dir; });
        
        if (it == local_entries.end()) {
            std::cerr << "dir " << dir.toStdString() << " must be downloaded from server" << std::endl;
            actions.push_back(action_t(action_t::download_dir,
                db_entry_t(),
                localdir,
                remotedir,
                QFileInfo(),
                *find_resource(dir)));              
        }
        else if (!it->isDir()) {
            std::cerr << "dir " << dir.toStdString() << " already exists localy - BUT NOT A DIR" << std::endl;
            actions.push_back(action_t(action_t::error,
                localdb.entry(localdir),
                localdir,
                remotedir,
                *find_info(dir),
                *find_resource(dir)));               
        }
        else {
            assert(!"impossible");
        }
    }  
    
    return actions;
}

int main(int argc, char** argv)
{
    try {
    
        if (ne_sock_init() != 0)
            throw std::runtime_error("Can't init");

        std::cerr << "1" << std::endl;
        
        
        session_t session("https", "webdav.yandex.ru");
        
        std::cerr << "2" << std::endl;
        
        session.set_auth("", "");
        
        std::cerr << "3" << std::endl;
        
        session.set_ssl();
        
        std::cerr << "4" << std::endl;
        
        session.open();
        
        std::cerr << "5" << std::endl;

        propfind_context ctx;
        ctx.path = "/";
        ctx.results = NULL;

        std::cerr << "10" << std::endl;
        
        char *espath = ne_path_escape("/testf/games");

        auto l = session.ls(espath);
        
        std::cerr << "a1" << std::endl;
        BOOST_FOREACH(auto s, l) {
//             std::cerr << "file:" << s << std::endl;
        }
        
        std::cerr << "a2" << std::endl;
        
//         BOOST_FOREACH(auto s, get_res(session.session(), espath)) {
//             std::cerr << "file path:" << s.path << std::endl;
//             std::cerr << "file name:" << s.name << std::endl;
// //             std::cerr << "file stat:" << s.size << std::endl;
// //             std::cerr << "file dir:" << s.dir << std::endl;
// //             std::cerr << "file ctime:" << s.ctime << std::endl;
// //             std::cerr << "file mtime:" << s.mtime << std::endl;
// //             std::cerr << "file exec:" << s.exec << std::endl;
// //             std::cerr << "file etag:" << s.etag << std::endl;
//         }
//         
        std::cerr << "a3" << std::endl;
        
    //     std::cerr << "11" << std::endl;
    //     
    //     ne_propfind_handler *ph = ne_propfind_create(session, espath, NE_DEPTH_ONE);
    // 
    //     std::cerr << "12" << std::endl;
    //     
    //     ret = ne_propfind_named(ph, prop_names, prop_result, &ctx);
    // 
    //     std::cerr << "13" << std::endl;
    //     
    // //     ret = get_error(ret, "PROPFIND");
    //     ne_propfind_destroy(ph);
        free(espath);

        std::cerr << "14" << std::endl;
        
        
        int f = open("/tmp/333", O_RDWR | O_CREAT | O_TRUNC);
        
        session_t::ContentHandler writer = [f] (const char* buf, size_t len) {return write(f, buf, len);};
        
        session.get("/testf/games/русские буквы.txt", writer);
        
//         session.put("/testf/games/русские self.txt", {'a', 'b', 'c'});


        const QString localfolder = "/tmp/111";
        const QString remotefolder = "/1";


        qDebug() << "====== LOAD ======";        
        db_t localdb(localfolder + "/" + db_t::prefix + "/db", localfolder);
//         localdb.load(localfolder + "/.davqt/db", localfolder);
        
        auto act = handle_dir(localdb, session, localfolder, remotefolder);
        std::cerr << std::endl << "actions:" << std::endl;
        BOOST_FOREACH(action_t a, act) {
            switch (a.type) {
                case action_t::upload : std::cerr << "upload from " << a.local.absoluteFilePath().toStdString() << " to " << a.remote.path << std::endl; break;
                case action_t::download : std::cerr << "download to " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::local_changed : std::cerr << "local_changed file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::remote_changed : std::cerr << "remote_changed file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::unchanged : std::cerr << "unchanged file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::conflict : std::cerr << "CONFLICT file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::both_deleted : std::cerr << "both delete " << a.local_file.toStdString() << " and " << a.remote_file.toStdString() << std::endl; break;
                case action_t::local_deleted : std::cerr << "local delete " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
                case action_t::remote_deleted : std::cerr << "remote delete " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
                case action_t::upload_dir : std::cerr << "upload dir from " << a.local.absoluteFilePath().toStdString() << " to " << a.remote.path << std::endl; break;
                case action_t::download_dir : std::cerr << "download dir to " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
                case action_t::error : std::cerr << "error in " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
            }
        }

        action_processor_t processor(session, localdb);
        
        
        std::cerr << std::endl << "process:" << std::endl;
        BOOST_FOREACH(action_t a, act) {
            processor.process(a);
        }  
        
        
//         QDir d("/tmp/111");
//         
//         auto files = session.ls("/1");
//         
//         BOOST_FOREACH(auto f, files) {
//             std::cerr << "FILE:" << f <<std::endl;
//         }
//         
//         BOOST_FOREACH(const QFileInfo& info, d.entryInfoList()) {
//             if (!info.isFile()) continue;
//             std::cerr << "to sync:" << info.absoluteFilePath().toStdString() << std::endl;
//             
//             auto it = std::find(files.begin(), files.end(), info.fileName().toStdString());     
//             
//             if (it == files.end()) {
//                 std::cerr << "need to upload!" << std::endl;
//                 
//                 QFile f(info.absoluteFilePath());
//                 f.open(QIODevice::ReadOnly);
//                 const QByteArray data = f.readAll();
//                 const std::vector<char> d(data.begin(), data.end());
//                 session.put((QString("/1/") + info.fileName()).toStdString(), d);
//                 
//             } else {
//                 std::cerr << "need to sync" << std::endl;
//                 if (info.lastModified() == QDateTime::fromTime_t(session.mtime((QString("/1/") + info.fileName()).toStdString()))) {
//                     std::cerr << "time differs" << std::endl;
//                 }
//                 else {
//                     std::cerr << "time equal" << std::endl;
//                 }
//             }
//         }
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








