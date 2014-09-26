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
/*
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

    Q_SLOT void cancell();
    bool is_closed() const;
    
    QList<remote_res_t> get_resources(QString unescaped_path);
    
    
    /// download data to descriptor \p fd
    stat_t get(const QString& unescaped_path, int fd);

    /// upload data from descriptor \p fd
    stat_t put(const QString& unescaped_path, int fd);

    /// get resource attributes
    stat_t head(const QString& unescaped_path);

    stat_t set_permissions(const QString& unescaped_path, QFile::Permissions perms);
    
    void remove(const QString& unescaped_path);

    stat_t mkcol(const QString& unescaped_path);
   
Q_SIGNALS:
    void connected();
    void disconnected();
    void get_progress(qint64 progress, qint64 total);
    void put_progress(qint64 progress, qint64 total);

private:
    
private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};*/


#endif // DAVQT_SESSION_H

