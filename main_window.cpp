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
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QMenu>

#include "tools.h"

#include "ui_main_window.h"
#include "main_window.h"

struct main_window_t::Pimpl {
    Ui_main ui;
    sync_manager_t* manager;
    Actions actions;
};

main_window_t::main_window_t(QWidget* parent)
    : QWidget(parent)
    , p_(new Pimpl())
{
    p_->ui.setupUi(this);
    
    const QString localfolder = "/tmp/111";
    const QString remotefolder = "/1";    

    sync_manager_t::connection conn = {
        "https",
        "webdav.yandex.ru",
        -1,
        "",
        ""
    };
    
    p_->manager = new sync_manager_t(0, conn, localfolder, remotefolder);

    Q_VERIFY(connect(p_->manager, SIGNAL(status_updated(Actions)), this, SLOT(status_updated(Actions))));         
    
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_started()), this, SLOT(sync_started())));            
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_finished()), this, SLOT(sync_finished())));     
    
    Q_VERIFY(connect(p_->manager, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_error(action_t)), this, SLOT(action_error(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));
    
    Q_VERIFY(connect(p_->ui.update, SIGNAL(clicked()), p_->manager, SLOT(update_status())));
    Q_VERIFY(connect(p_->ui.sync, SIGNAL(clicked()), this, SLOT(sync())));
    
    QSystemTrayIcon* tray = new QSystemTrayIcon(QIcon("icons:sync.png"), this);
    tray->setVisible(true);
    
    QMenu* menu = new QMenu();

    ::connect(menu->addAction(QIcon("icons:prefs.png"), QObject::tr("Preferences")), SIGNAL(triggered(bool)), [] {

    });
    
    ::connect(menu->addAction(QIcon("icons:exit.png"), QObject::tr("Quit")), SIGNAL(triggered(bool)), [] {
        qApp->exit(0);
    });


    tray->setContextMenu(menu);
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(tray_activated(QSystemTrayIcon::ActivationReason)));
}

main_window_t::~main_window_t() {
    
}

void main_window_t::tray_activated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::QSystemTrayIcon::Trigger) {
        //This is very complicated magic... like voodoo!
        show();
        Qt::WindowFlags eFlags = windowFlags();
        eFlags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(eFlags);
        show();
        eFlags &= ~Qt::WindowStaysOnTopHint;
        setWindowFlags(eFlags);

        show();
        raise();
        activateWindow();
    }
}

void main_window_t::show_preferences()
{
    
}

void main_window_t::sync()
{
    p_->manager->sync(p_->actions);
}

QProgressBar* get_pb(QTreeWidget* tree, QTreeWidgetItem* item) {
    QProgressBar* pb = qobject_cast<QProgressBar*>(tree->itemWidget(item, 2));
//     Q_ASSERT(pb);
    return pb;
}

void main_window_t::status_updated(const Actions& actions)
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
                p_->ui.actions->setItemWidget(group, 2, new QProgressBar());                
                if (QProgressBar* pb = get_pb(p_->ui.actions, group)) {
                    pb->setMinimum(0);
                    pb->setMaximum(0);
                    pb->setValue(0);
                }
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
            case action_t::unchanged:       return find(tr("Unchanged"));
            case action_t::conflict:        return find(tr("Conflicts"));
            case action_t::both_deleted:    return find(tr("Deleted"));
            case action_t::local_deleted:   return find(tr("Locally deleted"));
            case action_t::remote_deleted:  return find(tr("Remotely deleted"));
            case action_t::upload_dir:      return find(tr("Folders to upload"));
            case action_t::download_dir:    return find(tr("Folders to download"));
            default: Q_ASSERT(!"unhandled action type");
        };
        return NULL;
    };
    
    Q_FOREACH(const action_t& action, actions) {
        QTreeWidgetItem* group = groupit(action.type);
        if (QProgressBar* pb = get_pb(p_->ui.actions, group)) {
            pb->setMaximum(pb->maximum() + 1);
        }
        QTreeWidgetItem* item = new QTreeWidgetItem(group, QStringList() << action.local_file << tr("Not synced"));
    }

    p_->ui.actions->resizeColumnToContents(0);
    p_->ui.actions->resizeColumnToContents(1);
    p_->ui.actions->resizeColumnToContents(2);
    
    auto it = groupit(action_t::unchanged);
    if (it->childCount())
        it->setExpanded(false);
    else
        delete it;
    
    p_->actions = actions;
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
    p_->ui.actions->setItemWidget(it, 2, new QProgressBar());                
    if (QProgressBar* pb = get_pb(p_->ui.actions, it)) {
        pb->setMinimum(0);
        pb->setMaximum(0);
        pb->setValue(0);    
    }
}


void main_window_t::action_success(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    it->setText(1, tr("Completed"));
    action_finished(action);    
}

void main_window_t::action_error(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    it->setText(1, tr("Error"));
    action_finished(action);    
}

void main_window_t::action_progress(const action_t& action, qint64 progress, qint64 total)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);    
    
    if (QProgressBar* pb = get_pb(p_->ui.actions, it)) {
        if (total) pb->setMaximum(total);
        pb->setValue(progress);
    }
}

void main_window_t::action_finished(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);    
    if (QProgressBar* pb = get_pb(p_->ui.actions, it->parent())) {
        pb->setValue(pb->value() + 1);
    }

    if (QProgressBar* pb = get_pb(p_->ui.actions, it)) {
        pb->deleteLater();
        p_->ui.actions->setItemWidget(it, 2, NULL);
    };    
}

void main_window_t::sync_started()
{
    p_->ui.sync->setEnabled(false);
    qDebug() << "! =========== started!";
}

void main_window_t::sync_finished()
{
    p_->ui.sync->setEnabled(true);
    qDebug() << "! =========== completed";
}




