
#pragma once

#include <string>
#include <stdexcept>

#include <QString>
#include <QFileInfo>

struct stat_t {
    stat_t(const std::string& e, time_t lt, time_t rt, off_t s)
        : etag(e), local_mtime(lt), remote_mtime(rt), size(s) {}
    
    stat_t() : local_mtime(0), remote_mtime(0), size(-1) {}
    
    bool empty() const {return local_mtime == 0 && remote_mtime == 0 && size == -1;}
    
    std::string etag;
    
    time_t local_mtime;
    time_t remote_mtime;
    off_t size;
};

/// describes last synx state of local and remote file
struct db_entry_t {
    
 db_entry_t(const QString& r, const QString& f, const QString& n, const std::string& e, time_t lt, time_t rt, off_t s)
        : root(r), folder(f), name(n), stat(e, lt, rt, s) {}
    
    db_entry_t() {}
    
    bool empty() const {return root.isEmpty() && folder.isEmpty() && name.isEmpty() && stat.empty();}
    
    QString root;
    QString folder;
    QString name;
    
    stat_t stat;
};

/// describes state of local resource
typedef QFileInfo local_res_t;

/// describes state of remote resource
struct remote_res_t {
    
    std::string path;   /* The unescaped path of the resource. */
    std::string name;   /* The name of the file or directory. Only the last
                           component (no path), no slashes. */
    std::string etag;   /* The etag string, including quotation characters,
                           but without the mark for weak etags. */
    bool dir;
                           
    off_t size;         /* File size in bytes (regular files only). */
    time_t ctime;       /* Creation date. */
    time_t mtime;       /* Date of last modification. */

    enum exec_e {
        none,
        executable,
        not_executable,
    } exec;
    
    inline bool empty() const {return path.empty() && name.empty();}
};

/// describes action needed to perform
struct action_t {
    typedef int TypeMask;
    
    enum type_e {
        error         = 0,        
        upload        = 1 << 0,
        download      = 1 << 1,
        local_changed = 1 << 2,
        remote_changed= 1 << 3,
        unchanged     = 1 << 4,
        conflict      = 1 << 5,
        both_deleted  = 1 << 6,
        local_deleted = 1 << 7,
        remote_deleted= 1 << 8,
        upload_dir    = 1 << 9,
        download_dir  = 1 << 10,
    } type;
    
    action_t(type_e t, const db_entry_t& e, const QString& lf, const QString& rf, const local_res_t& l, const remote_res_t& r)
        : type(t), dbentry(e), local_file(lf), remote_file(rf), local(l), remote(r)
    {}

    action_t() {}
    
    db_entry_t dbentry;
    
    QString local_file;
    QString remote_file;
    
    local_res_t local;
    remote_res_t remote;
};


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


