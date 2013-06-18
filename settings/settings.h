#pragma once

#include <QCoreApplication>
#include <QSettings>
#include <QObject>

#define GENERATE_PARAM(name, type, def) \
    type name() const {\
        return s_.value(#name, default_##name()).value<type>();\
    }\
    void set_##name(const type& value) {\
        s_.setValue(#name, value);\
    }\
    void reset_##name() {\
        set_##name(default_##name());\
    } \
    type default_##name() const {return def;}

class settings : public QObject
{
    Q_OBJECT
public:
    explicit settings(QObject* parent = 0)
        : QObject(parent)
        , s_(QSettings::IniFormat, QSettings::UserScope, qApp->organizationName(), qApp->applicationName())
    {}
    
    virtual ~settings(){};
    
    GENERATE_PARAM(username, QString, QString());
    GENERATE_PARAM(password, QString, QString());
    GENERATE_PARAM(host, QString, QString());
    GENERATE_PARAM(interval, int, 0);
    GENERATE_PARAM(remotefolder, QString, "/");
    GENERATE_PARAM(localfolder, QString, ".davqtfolder");
    
    

private:
    QSettings s_;
};