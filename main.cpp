
#include <iostream>
#include <memory>
#include <stdexcept>

#include <neon/ne_socket.h>

#include <QApplication>
#include <QTextCodec>

#include "settings/settings.h"
#include "database/fs/fs.h"
#include "main_window.h"
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

        const QString path = settings().remotefolder();
        storage_t storage("/tmp/davroot/files", path);
        database_p db(new database::fs_t(storage, "/tmp/davroot/db"));
        
        main_window_t m(db);
        m.show();
        
        return app.exec();
        
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








