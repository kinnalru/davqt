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


#ifndef DAVQT_SESSION_H
#define DAVQT_SESSION_H

#include <memory>

#include <QObject>

#include "types.h"

class session_t : public QObject
{
    Q_OBJECT
public:
    
    session_t(QObject* parent, const QString& schema, const QString& host, quint32 port = -1);
    ~session_t();
    
    session_t(const session_t&) = delete;
    session_t& operator=(const session_t&) = delete;
   
    void set_auth(const QString& user, const QString& password);
    void set_ssl();
    void open();    
    
    std::vector<remote_res_t> get_resources(const QString& path);
    
    stat_t get(const QString& unescaped_path, int fd);
    stat_t put(const QString& unescaped_path, int fd);

    void head(const QString& unescaped_path, QString& etag, time_t& mtime, off_t& length);

    void remove(const QString& unescaped_path);

    void mkcol(const QString& unescaped_path);
   
Q_SIGNALS:
    void connected();
    void disconnected();
    void get_progress(qint64 progress, qint64 total);
    void put_progress(qint64 progress, qint64 total);

private:
    static void notifier(void *userdata, int status_int, const void* raw_info);
   
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};


#endif // DAVQT_SESSION_H

