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
#include <QUrl>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QMenu>

#include "3rdparty/preferences/src/preferences_dialog.h"

#include "tools.h"

#include "settings/main_settings.h"
#include "settings/settings.h"

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

    
    Q_VERIFY(connect(p_->ui.sync, SIGNAL(clicked(bool)), this, SLOT(sync())));

    
    QMenu* menu = new QMenu();

    ::connect(menu->addAction(QIcon("icons:prefs.png"), QObject::tr("Settings")), SIGNAL(triggered(bool)), [this] {
        preferences_dialog d(preferences_dialog::Auto, true, this);
        d.setWindowTitle(tr("Settings"));
        d.setWindowIcon(QIcon("icons:prefs.png"));
        d.add_item(new main_settings_t());
        d.exec();
        restart();
    });
    
    QAction* enabled = menu->addAction(QObject::tr("Enabled"));
    enabled->setCheckable(true);
    enabled->setChecked(settings().enabled());
    
    ::connect(enabled, SIGNAL(triggered(bool)), [this, enabled] {
        settings().set_enabled(enabled->isChecked());
    });

    connect(settings_impl_t::instance(), SIGNAL(enabled_changed(bool)), enabled, SLOT(setChecked(bool)));
    
    ::connect(menu->addAction(QIcon("icons:exit.png"), QObject::tr("Quit")), SIGNAL(triggered(bool)), [] {
        qApp->exit(0);
    });

    QSystemTrayIcon* tray = new QSystemTrayIcon(QIcon("icons:sync.png"), this);
    tray->setVisible(true);    
    tray->setContextMenu(menu);
    connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(tray_activated(QSystemTrayIcon::ActivationReason)));
    
    restart();
}

main_window_t::~main_window_t() {
    
}

void main_window_t::restart()
{
    if (p_->manager) {
        p_->manager->disconnect(this);
        p_->manager->disconnect();
        p_->manager->deleteLater();
        p_->ui.actions->clear();
    }

    QUrl url(settings().host());
    url.setPath(settings().remotefolder());
    
    p_->ui.remotefolder->setText(url.toString());
    p_->ui.localfolder->setText(settings().localfolder());
    
    sync_manager_t::connection conn = {
        url.scheme(),
        url.host(),
        url.port(),
        settings().username(),
        settings().password()
    };
    
    p_->manager = new sync_manager_t(0, conn, settings().localfolder(), settings().remotefolder());

    Q_VERIFY(connect(p_->manager, SIGNAL(status_updated(Actions)), this, SLOT(status_updated(Actions))));         
    
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_started()), this, SLOT(sync_started())));            
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_finished()), this, SLOT(sync_finished())));     
    
    Q_VERIFY(connect(p_->manager, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_error(action_t,QString)), this, SLOT(action_error(action_t,QString))));
    Q_VERIFY(connect(p_->manager, SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));
    
    
    Q_VERIFY(::connect(p_->manager, SIGNAL(sync_started()), [this] {
        p_->ui.status->setPixmap(QPixmap("icons:state-sync.png"));
        p_->ui.sync->setEnabled(false);
    }));
    
    Q_VERIFY(::connect(p_->manager, SIGNAL(sync_finished()), [this] {
        p_->ui.status->setPixmap(QPixmap("icons:state-ok.png"));
        p_->ui.sync->setEnabled(true);
    }));
    
    
    
//     Q_VERIFY(::connect(p_->manager, SIGNAL(busy()), [this] {p_->ui.update->setEnabled(false);}));
//     Q_VERIFY(::connect(p_->manager, SIGNAL(ready()), [this] {p_->ui.update->setEnabled(true);}));
//     
//     Q_VERIFY(::connect(p_->manager, SIGNAL(busy()), [this] {p_->ui.sync->setEnabled(false);}));
//     Q_VERIFY(::connect(p_->manager, SIGNAL(ready()), [this] {p_->ui.sync->setEnabled(true);}));
    
//     Q_VERIFY(connect(p_->ui.update, SIGNAL(clicked()), p_->manager, SLOT(update_status())));
    
    QTimer::singleShot(std::max(settings().interval() * 1000, 10000), this, SLOT(sync()));
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

void main_window_t::sync()
{
    static bool guard = false;
    if (settings().enabled() && !guard && settings().interval() > 0) {
        guard = true;
        Q_VERIFY(::connectOnce(p_->manager, SIGNAL(status_updated(Actions)), [this] {
            start_sync();
        }));
        Q_VERIFY(::connectOnce(p_->manager, SIGNAL(sync_finished()), [this, &guard] {
            guard = false;
        }));   
        p_->manager->update_status();
    }
    
    QTimer::singleShot(std::max(settings().interval() * 1000, 10000), this, SLOT(sync()));
}

void main_window_t::start_sync()
{
    p_->manager->sync(p_->actions);
}


QProgressBar* get_pb(QTreeWidget* tree, QTreeWidgetItem* item) {
    QProgressBar* pb = qobject_cast<QProgressBar*>(tree->itemWidget(item, 2));
//     Q_ASSERT(pb);
    return pb;
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

void main_window_t::status_updated(const Actions& actions)
{
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
    
    auto allitems = all_items(p_->ui.actions);
    
    Q_FOREACH(const action_t& action, actions) {
        QTreeWidgetItem* group = groupit(action.type);
        if (QProgressBar* pb = get_pb(p_->ui.actions, group)) {
            pb->setMaximum(pb->maximum() + 1);
        }
        QTreeWidgetItem* item = find_item(p_->ui.actions, action.local_file);
        if (!item) item = new QTreeWidgetItem(group, QStringList() << action.local_file << tr("Not synced"));
        
        if (item->parent() != group) {
            item = item->parent()->takeChild(item->parent()->indexOfChild(item));
            group->addChild(item);
        }
        allitems.removeAll(group);
        allitems.removeAll(item);
    }

    Q_FOREACH(QTreeWidgetItem* item, allitems) {
        int index = p_->ui.actions->indexOfTopLevelItem(item);
        if (index != -1) {
            delete p_->ui.actions->takeTopLevelItem(index);
        }
        else {
            delete item->parent()->takeChild(item->parent()->indexOfChild(item));
        }
    }
    
    p_->ui.actions->resizeColumnToContents(0);
    p_->ui.actions->resizeColumnToContents(1);
    p_->ui.actions->resizeColumnToContents(2);
    
    p_->actions = actions;
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
        pb->setFormat("%v of %m (%p%)");
    }
}


void main_window_t::action_success(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    it->setText(1, tr("Completed"));
    action_finished(action);    
}

void main_window_t::action_error(const action_t& action, const QString& message)
{
    auto it = find_item(p_->ui.actions, action.local_file);
    Q_ASSERT(it);
    it->setText(1, tr("Error"));
    action_finished(action);    
    it = find_item(p_->ui.errors, action.local_file);
    if (!it) {
        QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << action.local_file << message);
    }
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
    qDebug() << "! =========== started!";
}

void main_window_t::sync_finished()
{
    qDebug() << "! =========== completed";
}




