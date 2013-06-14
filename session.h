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

#include "types.h"

class session_t
{
public:
    
    typedef std::function<size_t (const char*, size_t)> ContentHandler;
    
    session_t(const std::string& schema, const std::string& host, unsigned int port = -1);
    ~session_t();
   
    void set_auth(const std::string& user, const std::string& password);
    void set_ssl();
    void open();    
    
    std::vector<remote_res_t> get_resources(const std::string& path);
    
    void get(const std::string& path_raw, ContentHandler& handler);
    stat_t get(const std::string& path_raw, int fd);
    
    void put(const std::string& path_raw, const std::vector<char>& buffer);
    stat_t put(const std::string& path_raw, int fd);

    void head(const std::string& path_raw, std::string& etag, time_t& mtime, off_t& length);

    void remove(const std::string& path_raw);

    void mkcol(const std::string& path_raw);
   
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};


#endif // SESSION_H
