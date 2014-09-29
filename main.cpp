
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

struct application: public QApplication {

    application(int& argc, char** argv)
        : QApplication(argc, argv)
    {}

    virtual bool notify(QObject* obj, QEvent* event)
    {
        try {
            return QApplication::notify(obj, event);
        }
        catch(std::exception& e) {
            qDebug() << "Unhandled exception in notify: " << e.what();
            qDebug() << "In object:" << obj->staticMetaObject.className() << obj->objectName() << "Event:" << event->type();
            Q_ASSERT(false);
        }
        return false;
    }
};

int main(int argc, char** argv)
{
    try {
        if (ne_sock_init() != 0)
            throw std::runtime_error("Can't init libneon");

        application app(argc, argv);
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








