#include <QApplication>
#include "provisor.h"
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QTextCodec::setCodecForTr( QTextCodec::codecForName("utf8") );
    QTextCodec::setCodecForCStrings ( QTextCodec::codecForName("utf8") );
    provisor w;
    w.show();
    
    return a.exec();
}
