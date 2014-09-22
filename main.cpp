
#include <iostream>
#include <stdexcept>

#include <neon/ne_socket.h>

#include <QApplication>
#include <QTextCodec>

#include "main_window.h"

#include "database/fs/fs.h"
#include "types.h"

int main(int argc, char** argv)
{
    try {
        if (ne_sock_init() != 0)
            throw std::runtime_error("Can't init libneon");

        QApplication app(argc, argv);
   
        QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));

        QDir::addSearchPath("icons", QString(":icons/images/"));
        QDir::addSearchPath("images", QString(":icons/images/"));

//         QApplication::setQuitOnLastWindowClosed(false);


        auto db = new database::fs_t("/tmp/1");
        
        db->put("test", db_entry_t("r", "f", "n", stat_t("etag", "local_path", 123, QFile::ReadOther, 333), stat_t("etag", "remote_path", 123, QFile::ReadOther, 444), false));
        
        main_window_t m;
        m.show();
        
        return app.exec();
        
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








