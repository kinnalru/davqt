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

#include <memory>
#include <string>
#include <functional>

#include <bits/stl_vector.h>
#include <boost/noncopyable.hpp>

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

class session_t
{
public:
    
    typedef std::function<size_t (const char*, size_t)> ContentHandler;
    
    session_t(const std::string& schema, const std::string& host, unsigned int port = -1);
    ~session_t();
   
    std::vector<std::string> ls(const std::string& path);
    
    void get(const std::string& path, ContentHandler& handler);
    
   
    ne_session* session() const;
    
private:
    std::vector<resource_t> get_resources(const std::string& path);
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};

#endif // SESSION_H
