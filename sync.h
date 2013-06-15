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

class sync_manager_t : public QObject, public QRunnable {
    Q_OBJECT
public:
    
    sync_manager_t(QObject* parent);
    virtual ~sync_manager_t();
    
    void start_sync(const QString& localfolder, const QString& remotefolder);
    void start_part(const QString& localfolder, const QString& remotefolder);

    virtual void run();
    
Q_SIGNALS:
    void sync_started(const QList<action_t>& actions);
    void action_started(const action_t& action);
    void action_success(const action_t& action);
    void action_error(const action_t& action);
    void sync_finished();
    
    void action_progress(const action_t& action, qint64 progress, qint64 total);

    
protected Q_SLOTS:
    void get_progress(qint64 progress, qint64 total);
    void put_progress(qint64 progress, qint64 total);
    
private:
    QString lf_;
    QString rf_;
    bool start;
    
    action_t current_;
    
};



#endif // DAVQT_SYNC_H
