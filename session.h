/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2013  <copyright holder> <email>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#ifndef SESSION_H
#define SESSION_H

#include <QString>
#include <QFileInfo>
#include <memory>
#include <string>
#include <functional>

#include <boost/noncopyable.hpp>

#include "sync.h"

enum exec_e {
    none,
    exec,
    no_exec,
};

struct resource_t {
    std::string path;   /* The unescaped path of the resource. */
    std::string name;   /* The name of the file or directory. Only the last
                           component (no path), no slashes. */
    std::string etag;   /* The etag string, including quotation characters,
                           but without the mark for weak etags. */
    bool dir;
                           
    off_t size;         /* File size in bytes (regular files only). */
    time_t ctime;       /* Creation date. */
    time_t mtime;       /* Date of last modification. */

    exec_e exec;        
};

struct action_t {
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
    };
    
    typedef int TypeMask;
    
    type_e type;
    QString localpath;
    QFileInfo localinfo;
    
    QString remotepath;
};

struct stat_t {
    time_t local_mtime;
    time_t remote_mtime;
    off_t size;
    std::string etag;
};

class session_t
{
public:
    
    typedef std::function<size_t (const char*, size_t)> ContentHandler;
    
    session_t(const std::string& schema, const std::string& host, unsigned int port = -1);
    ~session_t();
   
    std::vector<std::string> ls(const std::string& path);
    
    time_t mtime(const std::string& path);
    
    std::vector<resource_t> get_resources(const std::string& path);
    
    void get(const std::string& path, ContentHandler& handler);
    stat_t get(const std::string& path_raw, const QString& localpath);
    
    void put(const std::string& path, const std::vector<char>& buffer);
    stat_t put(const std::string& path_raw, const QString& localpath);
//     void put(const std::string& path_raw, const QString& path);
   
    void head(const std::string& raw_path, std::string& etag, time_t& mtime, off_t& length);
    
    
    ne_session* session() const;
    
private:
    
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};


class action_processor_t {
public:
    action_processor_t(session_t& session);
    
    void process(const action_t& action);

private:
    session_t& session_;
};


#endif // SESSION_H
