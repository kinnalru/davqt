
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
#include "main_window.h"

#include <QApplication>


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
        
        
//         auto act = handle_dir(localdb, session, localfolder, remotefolder);
//         std::cerr << std::endl << "actions:" << std::endl;
//         BOOST_FOREACH(action_t a, act) {
//             switch (a.type) {
//                 case action_t::upload : std::cerr << "upload from " << a.local.absoluteFilePath().toStdString() << " to " << a.remote.path << std::endl; break;
//                 case action_t::download : std::cerr << "download to " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::local_changed : std::cerr << "local_changed file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::remote_changed : std::cerr << "remote_changed file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::unchanged : std::cerr << "unchanged file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::conflict : std::cerr << "CONFLICT file " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::both_deleted : std::cerr << "both delete " << a.local_file.toStdString() << " and " << a.remote_file.toStdString() << std::endl; break;
//                 case action_t::local_deleted : std::cerr << "local delete " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
//                 case action_t::remote_deleted : std::cerr << "remote delete " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
//                 case action_t::upload_dir : std::cerr << "upload dir from " << a.local.absoluteFilePath().toStdString() << " to " << a.remote.path << std::endl; break;
//                 case action_t::download_dir : std::cerr << "download dir to " << a.local.absoluteFilePath().toStdString() << " from " << a.remote.path << std::endl; break;
//                 case action_t::error : std::cerr << "error in " << a.local.absoluteFilePath().toStdString() << " and " << a.remote.path << std::endl; break;
//                 default: Q_ASSERT(!"wtfo");
//             }
//         }
// 
//         action_processor_t processor(session, localdb);
//         
//         
//         std::cerr << std::endl << "process:" << std::endl;
//         BOOST_FOREACH(action_t a, act) {
//             processor.process(a);
//         }  
//    

        QApplication app(argc, argv);
   
        main_window_t m;
        m.show();
        
        return app.exec();
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








