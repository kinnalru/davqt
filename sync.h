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


#ifndef DAVQT_SYNC_H
#define DAVQT_SYNC_H

#include <QList>
#include <QRunnable>
#include <QThreadPool>

#include "types.h"
#include "database.h"

class thread_pool_t : public QThreadPool {
    Q_OBJECT
public:
    thread_pool_t(QObject* parent = 0);
    virtual ~thread_pool_t() {};
    
    void start(QRunnable* runnable);
    
Q_SIGNALS:
    void busy();
    void ready();
private:
    bool busy_;
};

class sync_manager_t : public QObject {
    Q_OBJECT
public:
    
    struct connection {
        QString schema;
        QString host;
        quint32 port;
        QString login;
        QString password;
    };
    
    sync_manager_t(QObject* parent, connection conn, const QString& localfolder, const QString& remotefolder);
    virtual ~sync_manager_t();

static thread_pool_t* pool();

public Q_SLOTS:
    void update_status();
    void sync(const Actions& act);
    
    bool is_busy() const;
    
Q_SIGNALS:
    
    void busy();
    void ready();
    
    void status_updated(const Actions& actions);
    void status_error(const QString& error);
    
    void sync_started();
    void sync_finished();
    
    void action_started(const action_t& action);
    void action_success(const action_t& action);
    void action_error(const action_t& action);

    
    void progress(const action_t& action, qint64 progress, qint64 total);

private:
    const connection conn_;
    const QString lf_;
    const QString rf_;
    db_t localdb_;    
};


class progress_adapter_t : public QObject {
    Q_OBJECT
public: 
    explicit progress_adapter_t() {}
    void set_action(const action_t& a) {action_ = a;}
    
Q_SIGNALS:
    void progress(const action_t& action, qint64 progress, qint64 total);
    
public Q_SLOTS:
    void int_progress(qint64 prog, qint64 total) {
        Q_EMIT progress(action_, prog, (total) ? total : action_.local.size);
    }
private:
    action_t action_;
};


#endif // DAVQT_SYNC_H
