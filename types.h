
#pragma once

#include <string>
#include <stdexcept>

#include <QDebug>
#include <QString>
#include <QFileInfo>
#include <QDateTime>
#include <QVariant>
#include <QMetaObject>
#include <QMetaEnum>
#include <boost/concept_check.hpp>
#include "3rdparty/qtwebdav/webdav_url_info.h"

struct UrlInfo : public QUrlInfo {
  
  UrlInfo() {check();};
  UrlInfo(const UrlInfo& other) : QUrlInfo(other) {check();}
  UrlInfo(const QUrlInfo& info) : QUrlInfo(info) {check();}
  UrlInfo(const QFileInfo& info)
    : QUrlInfo(
      info.absoluteFilePath(),
      info.permissions(), info.owner(), info.group(),
      info.size(), info.lastModified().toUTC(), info.lastRead().toUTC(), 
      info.isDir(), info.isFile(), info.isSymLink(),
      info.isWritable(), info.isReadable(), info.isExecutable()
    )
  {
    check();
  }
    
  UrlInfo(const QVariantMap& data) {
    setName(data["name"].toString());
    setLastModified(data["lastModified"].toDateTime());
    setPermissions(data["permissions"].value<int>());
    setSize(data["size"].value<quint64>());
  }
    
  inline QVariantMap dump() const {
    QVariantMap data;
    data["name"]  = name();
    data["lastModified"] = lastModified();
    data["permissions"] = static_cast<int>(permissions());
    data["size"]  = size();
    return data;
  }
  
  bool empty() const {
    return (lastModified() == QDateTime()) || (size() == -1);
  }
  
  bool ready() const {
    return (lastModified() != QDateTime()) && (size() != -1) && (permissions() != 0);
  }
  
    
private: 
  void check() {
    if (isDir() && !name().endsWith("/")) {
      setName(name() + "/");
    }
  }
};

/// describes action needed to perform
class action_t {
    Q_GADGET
    Q_ENUMS(type_e)
    
public:
    typedef int TypeMask;
    
    enum type_e {
        error         = 0,        
        upload        = 1 << 0,
        download      = 1 << 1,
        compare       = 1 << 2,
        forgot        = 1 << 3,
        remove_local  = 1 << 4,
        remove_remote = 1 << 5,
        
        local_changed = 1 << 6,
        remote_changed= 1 << 7,
        unchanged     = 1 << 8,
        conflict      = 1 << 9,
        
        upload_dir    = 1 << 10,
        download_dir  = 1 << 11,
    } type;
    
    
    static QString type_text(type_e t) {
      QMetaEnum e = action_t::staticMetaObject.enumerator(action_t::staticMetaObject.indexOfEnumerator("type_e"));
      return e.valueToKey(t);
    }
    
    QString type_text() const {
      return action_t::type_text(type);
    }
    
    
    
    action_t(type_e t, const QString& key, const UrlInfo& l = UrlInfo(), const UrlInfo& r = UrlInfo())
        : type(t), key(key), local(l), remote(r)
    {}

    action_t() {}
    
    inline bool empty() const {return key.isEmpty();}
    
    inline bool operator==(const action_t& other) const {
        return type == other.type && key == other.key;
    }
    
    QString key;
    
    UrlInfo local;
    UrlInfo remote;
};

struct conflict_ctx {
  const action_t action;
  QString local_file;
  QString remote_file;
  QString result_file;
};

inline QDebug operator<<(QDebug dbg, const action_t &a)
{
    dbg.nospace() << "#action_t(" << "type:" << a.type_text() << ":" << a.key << ")";

    return dbg.space();
}

inline QDebug operator<<(QDebug dbg, const QFile::Permissions &perms)
{
    dbg.nospace() << "#Permissions(" << static_cast<int>(perms) << ")";

    return dbg.space();
}

struct connection_t {
  QString schema;
  QString host;
  quint32 port;
  QString login;
  QString password;
};

typedef QList<action_t> Actions;

class qt_exception_t : public std::runtime_error
{
    const QString msg;
public:
    explicit qt_exception_t(const QString& msg)
        : std::runtime_error(msg.toLocal8Bit().constData())
        , msg(msg) {}
        
    ~qt_exception_t() throw() {};

    const QString& message() const {return msg;}
};

class ne_exception_t : public std::runtime_error
{
    const int code_;
    const QString msg_;
public:
    explicit ne_exception_t(int code, const QString& msg)
        : std::runtime_error(msg.toLocal8Bit().constData())
        , code_(code), msg_(msg) {}
        
    ~ne_exception_t() throw() {};

    const QString& message() const {return msg_;}
    int code() const {return code_;}
};

class Package : public QObject {
  Q_OBJECT
    
public:
  template <typename F>
  Package(QObject* recepient, const F& func, Qt::ConnectionType type = Qt::QueuedConnection)
    : func_(func)
  {
    moveToThread(recepient->thread());
    QMetaObject::invokeMethod(this, "fire", type);
  }
    
  virtual ~Package() {};
    
private:
  Q_SLOT void fire() 
  {
    if (func_) func_();
    deleteLater();
  }
    
  std::function<void()> func_;
};
