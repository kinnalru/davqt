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


#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <memory>
#include <QWidget>

#include "sync.h"

class main_window_t : public QWidget
{
    Q_OBJECT
public:
    main_window_t(QWidget* parent = NULL);
    virtual ~main_window_t();
    
public Q_SLOTS:
    void sync();
    
    void status_updated(const Actions& actions);

    void sync_started();
    void sync_finished();
    
    void action_started(const action_t& action);
    void action_success(const action_t& action);
    void action_error(const action_t& action);
    
    void action_progress(const action_t& action, qint64 progress, qint64 total);        


private:
    void action_finished(const action_t& action);

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> p_;
};

#endif // MAIN_WINDOW_H
