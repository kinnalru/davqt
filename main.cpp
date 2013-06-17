
#include <iostream>
#include <stdexcept>

#include <neon/ne_socket.h>

#include <QApplication>
#include <QTextCodec>

#include "main_window.h"

int main(int argc, char** argv)
{
    try {
        if (ne_sock_init() != 0)
            throw std::runtime_error("Can't init libneon");

        QApplication app(argc, argv);
   
        QTextCodec::setCodecForTr(QTextCodec::codecForName("utf8"));

        QDir::addSearchPath("icons", QString(":icons/images/"));
        QDir::addSearchPath("images", QString(":icons/images/"));

        QApplication::setQuitOnLastWindowClosed(false);

        
        main_window_t m;
        m.show();
        
        return app.exec();
        
    }
    catch (std::exception& e) {
        std::cerr << "exception:" << e.what() << std::endl;
    }
    
    
    return 0;
}








