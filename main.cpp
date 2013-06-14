
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
                default: Q_ASSERT(!"wtfo");
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








