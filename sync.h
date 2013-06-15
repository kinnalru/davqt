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
#include <QThread>

#include "database.h"
#include "session.h"

action_t::type_e compare(const db_entry_t& dbentry, const local_res_t& local, const remote_res_t& remote);

QList<action_t> handle_dir(db_t& localdb, session_t& session, const QString& localfolder, const QString& remotefolder);


class sync_manager_t : public QThread {
    Q_OBJECT
public:
    
    sync_manager_t(QObject* parent);
    virtual ~sync_manager_t();
    
    void start_sync(const QString& localfolder, const QString& remotefolder);
    
Q_SIGNALS:
    void sync_started(const QList<action_t>& actions);
    void action_started(const action_t& action);
    void action_success(const action_t& action);
    void action_error(const action_t& action);
    void sync_finished();
    
protected:
    virtual void run();
    
private:
    QString lf;
    QString rf;
};



#endif // DAVQT_SYNC_H
