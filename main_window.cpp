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

#include "tools.h"

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
    Q_VERIFY(connect(manager, SIGNAL(sync_started(QList<action_t>)), this, SLOT(sync_started(QList<action_t>))));
    Q_VERIFY(connect(manager, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
    Q_VERIFY(connect(manager, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
    Q_VERIFY(connect(manager, SIGNAL(action_error(action_t)), this, SLOT(action_error(action_t))));
    Q_VERIFY(connect(manager, SIGNAL(sync_finished()), this, SLOT(sync_finished())));
    
    manager->start_sync(localfolder, remotefolder);
}

void main_window_t::sync_started(const QList<action_t>& actions)
{
    p_->ui.actions->clear();
    
    auto groupit = [this] (action_t::type_e type) -> QTreeWidgetItem* {
        auto find = [this] (const QString& text) {
            auto list = p_->ui.actions->findItems(text, Qt::MatchExactly);
            Q_ASSERT(list.size() <= 1);
            if (list.isEmpty()) {
                auto group = new QTreeWidgetItem(QStringList() << text);
                p_->ui.actions->addTopLevelItem(group);
                group->setExpanded(true);
                return group;
            } else {
                return list.front();
            }
        };
        
        switch(type) {
            case action_t::error:       return find(tr("Errors"));
            case action_t::upload:      return find(tr("Files to upload"));
            case action_t::download:        return find(tr("Files to download"));
            case action_t::local_changed:   return find(tr("Upload local changes"));
            case action_t::remote_changed:  return find(tr("Download remote changes"));
            case action_t::unchanged:   return find(tr("Unchanged"));
            case action_t::conflict:    return find(tr("Conflicts"));
            case action_t::both_deleted:    return find(tr("Deleted"));
            case action_t::local_deleted:   return find(tr("Locally deleted files"));
            case action_t::remote_deleted:  return find(tr("Remotely deleted files"));
            case action_t::upload_dir:      return find(tr("Folders to upload"));
            case action_t::download_dir:    return find(tr("Folders to download"));
            default: Q_ASSERT(!"unhandled action type");
        };
        return NULL;
    };
    
    Q_FOREACH(const action_t& action, actions) {
        QTreeWidgetItem* group = groupit(action.type);
        QTreeWidgetItem* item = new QTreeWidgetItem(group, QStringList() << action.local_file << tr("Not synced"));
    }
}


QList<QTreeWidgetItem*> all_items(const QTreeWidget* tree) {
    QList<QTreeWidgetItem*> ret;

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* it = tree->topLevelItem(i);
        ret << it;
        for (int c = 0; c < it->childCount(); ++ c) {
            ret << it->child(c);
        }
    }
    return ret;
}

QTreeWidgetItem* find_item(const QTreeWidget* tree, const QString& text) {
    Q_FOREACH(auto it, all_items(tree)) {
        if (it->text(0) == text) return it;
    }
    return NULL;
}

void main_window_t::action_started(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    
    it->setText(1, tr("In progress..."));
}


void main_window_t::action_success(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    
    it->setText(1, tr("Completed"));
}

void main_window_t::action_error(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    
    it->setText(1, tr("Error"));
}

void main_window_t::sync_finished()
{
    qDebug() << "completed";
}



