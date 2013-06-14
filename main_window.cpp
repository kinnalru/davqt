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

#include <QDebug>

#include "ui_main_window.h"
#include "main_window.h"

struct main_window_t::Pimpl {
    Ui_main ui;
};

main_window_t::main_window_t(QWidget* parent)
    : QWidget(parent)
    , p_(new Pimpl())
{
    p_->ui.setupUi(this);
    
    connect(p_->ui.sync, SIGNAL(clicked()), this, SLOT(sync()));
}

main_window_t::~main_window_t() {
    
}

void main_window_t::sync()
{
    const QString localfolder = "/tmp/111";
    const QString remotefolder = "/1";    

    sync_manager_t* manager = new sync_manager_t(0);
    Q_ASSERT(connect(manager, SIGNAL(sync_started(QList<action_t>)), this, SLOT(sync_started(QList<action_t>))));
    Q_ASSERT(connect(manager, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
    Q_ASSERT(connect(manager, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
    Q_ASSERT(connect(manager, SIGNAL(action_error(action_t)), this, SLOT(action_error(action_t))));
    Q_ASSERT(connect(manager, SIGNAL(sync_finished()), this, SLOT(sync_finished())));
    
    manager->start_sync(localfolder, remotefolder);
}

void main_window_t::sync_started(const QList<action_t>& actions)
{
    p_->ui.actions->clear();
    Q_FOREACH(const action_t& action, actions) {
        QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << action.local_file << "not started");
        p_->ui.actions->addTopLevelItem(item);
    }
}

void main_window_t::action_started(const action_t& action)
{
    auto list = p_->ui.actions->findItems(action.local_file, Qt::MatchExactly);
    Q_ASSERT(list.size() == 1);
    
    list.first()->setText(1, "inprogress...");
}


void main_window_t::action_success(const action_t& action)
{
    auto list = p_->ui.actions->findItems(action.local_file, Qt::MatchExactly);
    Q_ASSERT(list.size() == 1);
    
    list.first()->setText(1, "completed");
}

void main_window_t::action_error(const action_t& action)
{
    auto list = p_->ui.actions->findItems(action.local_file, Qt::MatchExactly);
    Q_ASSERT(list.size() == 1);
    
    list.first()->setText(1, "error");
}

void main_window_t::sync_finished()
{
    qDebug() << "completed";
}




