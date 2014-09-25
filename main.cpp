
#include <iostream>
#include <memory>
#include <stdexcept>

#include <neon/ne_socket.h>

#include <QApplication>
#include <QDesktopServices>
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
        app.setOrganizationName("gkb");
        app.setApplicationName("davqt");
   
        QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));

        QDir::addSearchPath("icons", QString(":icons/images/"));
        QDir::addSearchPath("images", QString(":icons/images/"));

        main_window_t m;
        m.show();
        
        return app.exec();
        
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








