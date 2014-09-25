#pragma once

#include <memory>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QSettings>
#include <QObject>
#include <QDebug>
#include <QDateTime>
#include <QDir>

#define GENERATE_PARAM(name, type, def) \
    type name() const {\
        return s_->value(#name, default_##name()).value<type>();\
    }\
    void set_##name(const type& value) {\
        s_->setValue(#name, value);\
        this->disconnect(instance());\
        if (this != instance()) {\
            connect(this, SIGNAL(name##_changed(type)), instance(), SIGNAL(name##_changed(type)));\
        }\
        Q_EMIT name##_changed(value);\
    }\
    void reset_##name() {\
        set_##name(default_##name());\
    } \
    type default_##name() const {\
        return def;\
    }

    
class settings_impl_t : public QObject {
    Q_OBJECT
public:
    explicit settings_impl_t() : QObject(0) {}
    virtual ~settings_impl_t() {};
    
    static settings_impl_t* instance() {
        static settings_impl_t* ptr = new settings_impl_t();
        return ptr;
    }
    
    GENERATE_PARAM(username, QString, QString());
    GENERATE_PARAM(password, QString, QString());
    GENERATE_PARAM(host, QString, QString());
    GENERATE_PARAM(interval, int, 0);
    GENERATE_PARAM(remotefolder, QString, "/");
    GENERATE_PARAM(localfolder, QString, settings_impl_t::data_path());
    GENERATE_PARAM(enabled, bool, false);
    GENERATE_PARAM(last_sync, QDateTime, QDateTime());
    
    static QString data_path() {
      const QString apppath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
      return QDir::cleanPath(apppath + QDir::separator() + "files");      
    }
    
Q_SIGNALS:
    void username_changed(const QString&);
    void password_changed(const QString&);
    void host_changed(const QString&);
    void interval_changed(int);
    void remotefolder_changed(const QString&);
    void localfolder_changed(const QString&);
    void enabled_changed(bool);
    void last_sync_changed(QDateTime);
    
protected:
    std::unique_ptr<QSettings> s_;
};

class settings : public settings_impl_t
{
    Q_OBJECT
public:
    explicit settings() : settings_impl_t()
    {
        s_.reset(new QSettings(QSettings::IniFormat, QSettings::UserScope, qApp->organizationName(), qApp->applicationName()));
    }
    
    virtual ~settings(){};

};

