
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

/// describes state of local resource
typedef QFileInfo local_res_t;

/// describes state of remote resource
struct remote_res_t {
    
    remote_res_t() : size(-1), ctime(0), mtime(0) {}
    
    QString path;   /* The unescaped path of the resource. */
    QString name;   /* The name of the file or directory. Only the last
                           component (no path), no slashes. */
    QString etag;   /* The etag string, including quotation characters,
                           but without the mark for weak etags. */
    bool dir;
                           
    qint64 size;         /* File size in bytes (regular files only). */
    time_t ctime;       /* Creation date. */
    time_t mtime;       /* Date of last modification. */

    QFile::Permissions perms;
    
    enum exec_e {
        none,
        executable,
        not_executable,
    } exec;
    
    inline bool empty() const {return path.isEmpty() && name.isEmpty();}
};

struct stat_t {
  stat_t(qlonglong m, QFile::Permissions p, quint64 s)
    : mtime(m), perms(p), size(s) {}
  
  stat_t()
    : mtime(0), size(-1), perms(0) {}
  
  stat_t(const local_res_t& info)
    : mtime(info.lastModified().toTime_t()), perms(info.permissions()), size(info.size()) {}
      
  stat_t(const remote_res_t& info)
    : mtime(info.mtime), perms(info.perms), size(info.size) {}
  
  stat_t(const QVariantMap& data) {
      mtime = data["mtime"].toLongLong();
      perms = QFile::Permissions(data["perms"].toInt());
      size = data["size"].toULongLong();
  }
  
  inline bool empty() const {return mtime == 0 && size == -1;}
  
//     inline bool operator==(const stat_t& other) const {
//         return mtime == other.mtime && perms == other.perms && size == other.size;
//     }
  
  inline QVariantMap dump() const {
      QVariantMap data;
      data["mtime"] = mtime;
      data["perms"] = static_cast<int>(perms);
      data["size"] = size;
      return data;
  }
  
  qlonglong mtime;
  QFile::Permissions perms;
  quint64 size;
};

/// describes last synx state of local and remote file
struct db_entry_t {
    
    db_entry_t(const QString& r, const QString& f, const QString& n, const stat_t& ls, const stat_t& rs, bool d)
        : root(r), folder(f), name(n), local(ls), remote(rs), dir(d), bad(false) {}
    
    db_entry_t() : dir(false), bad(false) {}
    
    bool empty() const {return root.isEmpty() && folder.isEmpty() && name.isEmpty() && local.empty() && remote.empty();}
    
    QString root;
    QString folder;
    QString name;
    
    stat_t local;    
    stat_t remote;
    
    bool dir;
    bool bad;
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
    
    
    
    action_t(type_e t, const QString& key, const stat_t& l, const stat_t& r)
        : type(t), key(key), local(l), remote(r)
    {}

    action_t() {}
    
//     inline bool empty() const {
//         return entry.isEmpty() && remote_file.isEmpty();
//     }
    
    inline bool operator==(const action_t& other) const {
        return type == other.type && key == other.key;
    }
    
    QString key;
    
    stat_t local;
    stat_t remote;
};

inline QDebug operator<<(QDebug dbg, const action_t &a)
{
    dbg.nospace() << "#action_t(" << "type:" << a.type << ":" << a.key << ")";

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

