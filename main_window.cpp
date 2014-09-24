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
#include <qmetaobject.h>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <qprogressdialog.h>
#include <QProcess>

#include "3rdparty/preferences/src/preferences_dialog.h"



#include "settings/main_settings.h"
#include "settings/settings.h"

#include "manager.h"

#include "tools.h"
#include "ui_main_window.h"
#include "main_window.h"

struct main_window_t::Pimpl {
    Ui_main ui;
    
    database_p db;
    
    std::unique_ptr<manager_t> manager;
    Actions actions;
    QSystemTrayIcon* tray;
    
    QAction* settings_a;
    QAction* sync_a;
    QAction* enabled_a;
};

QDateTime start_time_c;

main_window_t::main_window_t(database_p db)
    : QWidget(NULL)
    , p_(new Pimpl())
{
  
    static int r1 = qRegisterMetaType<action_t>("action_t");
    static int r2 = qRegisterMetaType<QList<action_t>>("QList<action_t>");    
    static int r3 = qRegisterMetaType<Actions>("Actions");   
  
    p_->db = db;
    p_->ui.setupUi(this);

    QMenu* menu = new QMenu();
    p_->tray = new QSystemTrayIcon(QIcon("icons:state-sync.png"), this);
    p_->tray->setVisible(true);    
    p_->tray->setContextMenu(menu);
    connect(p_->tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(tray_activated(QSystemTrayIcon::ActivationReason)));
    connect(p_->tray, SIGNAL(messageClicked()), this, SLOT(message_activated()));
    
    p_->settings_a = menu->addAction(QIcon("icons:settings.png"), QObject::tr("Settings"));
    p_->sync_a = menu->addAction(QIcon("icons:sync.png"), QObject::tr("Sync"));
    p_->enabled_a = menu->addAction(QObject::tr("Enabled"));
    
    ::connect(menu->addAction(QIcon("icons:exit.png"), QObject::tr("Quit")), SIGNAL(triggered(bool)), [] {
        qApp->exit(0);
    });
    
    Q_VERIFY(connect(p_->sync_a, SIGNAL(triggered(bool)), this, SLOT(force_sync())));
    Q_VERIFY(connect(settings_impl_t::instance(), SIGNAL(enabled_changed(bool)), p_->enabled_a, SLOT(setChecked(bool))));
    
    p_->ui.sync->setDefaultAction(p_->sync_a);
    p_->ui.settings->setDefaultAction(p_->settings_a);

    
    ::connect(p_->settings_a, SIGNAL(triggered(bool)), [this] {
        preferences_dialog d(preferences_dialog::Auto, true, this);
        d.setWindowTitle(p_->settings_a->text());
        d.setWindowIcon(p_->settings_a->icon());
        d.add_item(new main_settings_t());
        d.exec();
        
//         if (p_->manager->is_busy()) {
//             QMessageBox mb(QMessageBox::Warning, tr("Warning"), tr("Sync in progress. Do you want to wait untill finished?"),
//                 QMessageBox::Yes | QMessageBox::No
//             );
//             
//             Q_VERIFY(connect(p_->manager, SIGNAL(sync_finished()), &mb, SLOT(reject())));
//             QMessageBox::StandardButton result = QMessageBox::StandardButton(mb.exec());  
//             if (mb.clickedButton()) {
//                 if (result == QMessageBox::Yes) {
//                     QProgressDialog waiter(tr("Waiting for sync finished..."), tr("Break"), 0, 0);
//                     Q_VERIFY(connect(p_->manager, SIGNAL(sync_finished()), &waiter, SLOT(reset())));
//                     waiter.exec();
//                     if (waiter.wasCanceled()) {
//                         p_->manager->stop();
//                     }
//                 } else {
//                     p_->manager->stop();
//                 }         
//             }
//         }
//         restart();
    });
    
    p_->enabled_a->setCheckable(true);
    ::connect(p_->enabled_a, SIGNAL(toggled (bool)), [this] {
        settings().set_enabled(p_->enabled_a->isChecked());
        if (p_->enabled_a->isChecked()) {
            set_state(waiting);
        } 
        else {
            set_state(disabled);
        }
    });

    p_->enabled_a->setChecked(settings().enabled());
    

    restart();
    QTimer* timer = new QTimer(this);
    Q_VERIFY(connect(timer, SIGNAL(timeout()), this, SLOT(sync())));
    timer->setInterval(1000);

    start_time_c = QDateTime::currentDateTime();
    timer->start();
}

main_window_t::~main_window_t() {
    
}

void main_window_t::restart()
{
//     if (p_->manager) {
//         p_->manager->disconnect(this);
//         delete p_->manager;
//         p_->manager = NULL;
//     }

    url_t checker;
    
    const QUrl url(settings().host());
    
    p_->ui.remotefolder->setText(url.toString());
    p_->ui.localfolder->setText(settings().localfolder());
    
    thread_manager_t::connection conn = {
        url.scheme(),
        url.host(),
        url.port(),
        settings().username(),
        settings().password()
    };
    
/*    p_->manager = new thread_manager_t(0, p_->db, conn, settings().localfolder(), url.path() + settings().remotefolder());

    Q_VERIFY(connect(p_->manager, SIGNAL(status_updated(Actions)), this, SLOT(status_updated(Actions))));
    Q_VERIFY(connect(p_->manager, SIGNAL(status_error(QString)), this, SLOT(status_error(QString))));             
    
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_started()), this, SLOT(sync_started())));            
    Q_VERIFY(connect(p_->manager, SIGNAL(sync_finished()), this, SLOT(sync_finished())));     
    
    Q_VERIFY(connect(p_->manager, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
    Q_VERIFY(connect(p_->manager, SIGNAL(action_error(action_t,QString)), this, SLOT(action_error(action_t,QString))));
    Q_VERIFY(connect(p_->manager, SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));
    
    Q_VERIFY(::connect(p_->manager, SIGNAL(sync_finished()), [] {
        settings().set_last_sync(QDateTime::currentDateTime());
    }));  */ 
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

void main_window_t::message_activated()
{
    const QString local_file = p_->tray->property("file").toString();
    tray_activated(QSystemTrayIcon::QSystemTrayIcon::Trigger);
    if (!local_file.isNull()) {
        auto it = find_item(p_->ui.actions, local_file);
        Q_ASSERT(it);
        p_->ui.actions->setCurrentItem(it);
        
        it = find_item(p_->ui.errors, local_file);
        Q_ASSERT(it);
        p_->ui.errors->setCurrentItem(it);
    }
}

void main_window_t::sync()
{
    const int interval = settings().interval();
    const QDateTime last = settings().last_sync();

    qDebug() << last;
    
    int to_sync = (last.isValid())
      ? std::max(QDateTime::currentDateTime().secsTo(last.addSecs(interval)), 0)
      : 0;
    
    p_->ui.to_sync->setText(tr("(to sync: %1s)").arg(to_sync));
    p_->ui.last_sync->setText(tr("Last sync: %1").arg(last.toString()));

    if (to_sync != 0)
        return;
    
    
    if (start_time_c.secsTo(QDateTime::currentDateTime()) < 5) return;
    
    if (settings().enabled() && interval> 0)
        force_sync();
}

struct MyThread: public QThread {
  explicit MyThread() {
//     qDebug() << ">>>>>>>>>>>>>> THREAD CREATTED";
  }
  
    virtual ~MyThread() {
//       qDebug() << "<<<<< THREAD destroyed";
    }
  

};

void initialize_thread(QObject* object) {
  MyThread* th = new MyThread();
  Q_VERIFY(th->connect(th, SIGNAL(finished()), th, SLOT(deleteLater())));  
  th->start();
  object->moveToThread(th);
  Q_VERIFY(object->connect(object, SIGNAL(destroyed(QObject*)), th, SLOT(quit())));
}


#include "manager.h"


void main_window_t::force_sync()
{
  const QUrl url(settings().host());
  
  manager_t::connection conn = {
      url.scheme(),
      url.host(),
      url.port(),
      settings().username(),
      settings().password()
  };
  
  
  if(!p_->manager.get()) p_->manager.reset(new manager_t(p_->db, conn));
  
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(update_finished(Actions)), this, SLOT(status_updated(Actions))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(error(QString)),  this, SLOT(status_error(QString)))); 
  
  Q_VERIFY(::connectOnce(p_->manager.get(), SIGNAL(update_finished(Actions)), [this] {
    start_sync();
  }));
  
  p_->manager->start_update();
  
//   updater_t* u = new updater_t(p_->db, conn);
//   Q_VERIFY(connect(u, SIGNAL(finished()), u, SLOT(deleteLater())));  
//   initialize_thread(u);
//   
//   Q_VERIFY(connect(u, SIGNAL(status(Actions)), this, SLOT(status_updated(Actions))));
//   Q_VERIFY(connect(u, SIGNAL(error(QString)),  this, SLOT(status_error(QString)))); 
//   
//   Q_VERIFY(::connectOnce(u, SIGNAL(finished()), [this] {
//       start_sync();
//   }));
//   QTimer::singleShot(0, u, SLOT(start()));
}


void main_window_t::start_sync()
{
  const QUrl url(settings().host());
  
  manager_t::connection conn = {
      url.scheme(),
      url.host(),
      url.port(),
      settings().username(),
      settings().password()
  };
  
  if(!p_->manager.get()) p_->manager.reset(new manager_t(p_->db, conn));
  
//   if (p_->manager->status() != manager_t::free) {
//     QTimer::singleShot(100, this, SLOT(start_sync()));
//     return;
//   }
  
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_error(action_t,QString)), this, SLOT(action_error(action_t,QString))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));
  
  p_->manager->start_sync();
  
//   syncer_t* u = new syncer_t(p_->db, conn, p_->actions);
//   Q_VERIFY(connect(u, SIGNAL(finished()), u, SLOT(deleteLater())));  
//   initialize_thread(u);
//   
//   Q_VERIFY(connect(u, SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
//   Q_VERIFY(connect(u, SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
//   Q_VERIFY(connect(u, SIGNAL(action_error(action_t,QString)), this, SLOT(action_error(action_t,QString))));
//   Q_VERIFY(connect(u, SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));
// 
//   Q_VERIFY(::connectOnce(u, SIGNAL(finished()), [this] {
//       settings().set_last_sync(QDateTime::currentDateTime());
//   }));
//   
//   QTimer::singleShot(0, u, SLOT(start()));
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
            case action_t::error:          return find(tr("Errors"));
            case action_t::upload:         return find(tr("Files to upload"));
            case action_t::download:       return find(tr("Files to download"));
            case action_t::compare:        return find(tr("Files to compare"));
            case action_t::forgot:         return find(tr("Files to Forgot"));
            case action_t::remove_local:   return find(tr("Files to remove local"));
            case action_t::remove_remote:  return find(tr("Files to remove remote"));
            
            case action_t::local_changed:  return find(tr("Upload local changes"));
            case action_t::remote_changed: return find(tr("Download remote changes"));
            case action_t::unchanged:      return find(tr("Unchanged"));
            case action_t::conflict:       return find(tr("Conflicts"));
            
            case action_t::upload_dir:     return find(tr("Folders to upload"));
            case action_t::download_dir:   return find(tr("Folders to download"));
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
        QTreeWidgetItem* item = find_item(p_->ui.actions, action.key);
        if (!item) item = new QTreeWidgetItem(group, QStringList() << action.key << tr("Not synced"));
        
        if (item->parent() != group) {
            item = item->parent()->takeChild(item->parent()->indexOfChild(item));
            group->addChild(item);
        }
        allitems.removeAll(group);
        allitems.removeAll(item);
    }

    Q_FOREACH(QTreeWidgetItem* item, allitems) {
        int index = p_->ui.actions->indexOfTopLevelItem(item);
        if (index == -1) {
            allitems.removeAll(item);
            delete item->parent()->takeChild(item->parent()->indexOfChild(item));
        }
    }
    
    Q_FOREACH(QTreeWidgetItem* item, allitems) {
        int index = p_->ui.actions->indexOfTopLevelItem(item);
        delete p_->ui.actions->takeTopLevelItem(index);
    }
    
    auto unchanged = groupit(action_t::unchanged);
    unchanged = p_->ui.actions->takeTopLevelItem(p_->ui.actions->indexOfTopLevelItem(unchanged));
    p_->ui.actions->addTopLevelItem(unchanged);
    
    p_->ui.actions->resizeColumnToContents(0);
    p_->ui.actions->resizeColumnToContents(1);
    p_->ui.actions->resizeColumnToContents(2);
    
    p_->actions = actions;
}

void main_window_t::status_error(const QString& error)
{
    const auto text = tr("Can't update status");
    auto it = find_item(p_->ui.errors, text);
    if (!it) {
        QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << text << error);
        p_->ui.errors->addTopLevelItem(item);
    }
    
    set_state(gui_state_e::error);
}

void main_window_t::action_started(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.key);
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
    auto it = find_item(p_->ui.actions, action.key);
    Q_ASSERT(it);
    it->setText(1, tr("Completed"));
    action_finished(action);    
}

void main_window_t::action_error(const action_t& action, const QString& message)
{
    auto it = find_item(p_->ui.actions, action.key);
    Q_ASSERT(it);
    it->setText(1, tr("Error"));
    action_finished(action);    
    
    it->setTextColor(0, Qt::red);
    it->parent()->insertChild(0, it);
    it->treeWidget()->insertTopLevelItem(0, it->parent());
    
    it = find_item(p_->ui.errors, action.key);
    if (!it) {
        QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << action.key << message);
        p_->ui.errors->addTopLevelItem(item);
    }
    
    p_->tray->showMessage(tr("Error when sync file %1").arg(action.key), message, QSystemTrayIcon::Warning, 7000);
    p_->tray->setProperty("file", action.key);
    set_state(error);
}

void main_window_t::action_progress(const action_t& action, qint64 progress, qint64 total)
{
    auto it = find_item(p_->ui.actions, action.key);
    Q_ASSERT(it);    
    
    if (QProgressBar* pb = get_pb(p_->ui.actions, it)) {
        if (total) pb->setMaximum(total);
        pb->setValue(progress);
    }
}

void main_window_t::action_finished(const action_t& action)
{
    auto it = find_item(p_->ui.actions, action.key);
    Q_ASSERT(it);    
    
    it->setTextColor(0, QPalette().color(it->treeWidget()->foregroundRole()));
    
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
    set_state(syncing);
}

void main_window_t::sync_finished()
{
    set_state(complete);
}

void main_window_t::set_state(main_window_t::gui_state_e state)
{
    static gui_state_e last_state;
    const bool is_error = p_->ui.status->property("error").toBool();
    qDebug() << "is_error:" << is_error;
    switch(state) {
    case waiting: {
        qDebug() << "waitings";
        p_->ui.status->setPixmap(QPixmap("icons:state-pause.png"));
        p_->tray->setIcon(QIcon("icons:state-pause.png"));
        break;
    }
    case disabled: {
        qDebug() << "disabled";
        if (is_error) {
            p_->ui.status->setPixmap(QPixmap("icons:state-error.png"));            
            p_->tray->setIcon(QIcon("icons:state-error.png"));
        }
        else {
            p_->ui.status->setPixmap(QPixmap("icons:state-offline.png"));            
            p_->tray->setIcon(QIcon("icons:state-offline.png"));
        }
//         p_->manager->stop();        
        break;
    }
    case syncing: {
        qDebug() << "syncing";
        p_->ui.status->setProperty("error", false);        
        p_->ui.status->setPixmap(QPixmap("icons:state-sync.png"));
        p_->tray->setIcon(QIcon("icons:state-sync.png"));        
        p_->sync_a->setEnabled(false);
        break;
    }
    case complete: {
        qDebug() << "complete";
        if (is_error) {
            p_->ui.status->setPixmap(QPixmap("icons:state-error.png"));            
            p_->tray->setIcon(QIcon("icons:state-error.png"));
        }
        else {
            p_->ui.status->setPixmap(QPixmap("icons:state-ok.png"));
            p_->tray->setIcon(QIcon("icons:state-ok.png"));
        }
        p_->sync_a->setEnabled(true);
        
        break;
    }
    case error: {
        qDebug() << "error";
        if (last_state != syncing) {
            p_->ui.status->setPixmap(QPixmap("icons:state-error.png"));            
            p_->tray->setIcon(QIcon("icons:state-error.png"));
        }
        p_->ui.status->setProperty("error", true);
        break;
    }
    default: Q_ASSERT(!"unhandled state");
    }
    last_state = state;
}



