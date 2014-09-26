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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <qprogressdialog.h>
#include <QProcess>

#include "3rdparty/preferences/src/preferences_dialog.h"

#include "settings/main_settings.h"
#include "settings/sync_settings.h"
#include "settings/merge_settings.h"
#include "settings/settings.h"

#include "database/fs/fs.h"
#include "manager.h"

#include "tools.h"
#include "ui_main_window.h"
#include "main_window.h"

struct main_window_t::Pimpl {
    Ui_main ui;
    
    database_p db;
    std::unique_ptr<storage_t> storage;
    
    std::unique_ptr<manager_t> manager;
    Actions actions;
    QSystemTrayIcon* tray;
    
    QAction* settings_a;
    QAction* sync_a;
    QAction* enabled_a;
    
    gui_state_e state;
};

QDateTime start_time_c;

main_window_t::main_window_t()
    : QWidget(NULL)
    , p_(new Pimpl())
{
  
    static int r1 = qRegisterMetaType<action_t>("action_t");
    static int r2 = qRegisterMetaType<QList<action_t>>("QList<action_t>");    
    static int r3 = qRegisterMetaType<Actions>("Actions");   
  
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

    QHeaderView *verticalHeader = p_->ui.log->verticalHeader();
    verticalHeader->setResizeMode(QHeaderView::Fixed);
    verticalHeader->setDefaultSectionSize(16);
    
    
    
    ::connect(p_->settings_a, SIGNAL(triggered(bool)), [this] {
        preferences_dialog d(preferences_dialog::Auto, true, this);
        d.setWindowTitle(p_->settings_a->text());
        d.setWindowIcon(p_->settings_a->icon());
        d.add_item(new main_settings_t());
        d.add_item(new sync_settings_t());
        d.add_item(new merge_settings_t());
        d.exec();
       
        if (p_->manager->busy()) {
            QMessageBox mb(QMessageBox::Warning, tr("Warning"), tr("Sync in progress. Do you want to wait untill finished?"),
                QMessageBox::Yes | QMessageBox::No
            );
            
            Q_VERIFY(connect(p_->manager.get(), SIGNAL(finished()), &mb, SLOT(reject())));
            QMessageBox::StandardButton result = QMessageBox::StandardButton(mb.exec());  
            if (mb.clickedButton()) {
                if (result == QMessageBox::Yes) {
                    QProgressDialog waiter(tr("Waiting for sync finished..."), tr("Break"), 0, 0);
                    Q_VERIFY(connect(p_->manager.get(), SIGNAL(finished()), &waiter, SLOT(reset())));
                    waiter.exec();
                    if (waiter.wasCanceled()) {
                        p_->manager->stop();
                    }
                } else {
                    p_->manager->stop();
                }         
            }
        }
        restart();
    });
    
    ::connect(p_->enabled_a, SIGNAL(toggled (bool)), [this] {
        settings().set_enabled(p_->enabled_a->isChecked());
        if (p_->enabled_a->isChecked()) {
            set_state(waiting);
        } 
        else {
            set_state(disabled);
        }
    });

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
  if (p_->manager) {
    p_->manager->disconnect(this);
    p_->manager->stop();
    p_->manager->wait();
    p_->manager.reset();
  }
  
  const QString apppath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
  
  const QString prefix = settings().remotefolder();
  const QString dbpath = QDir::cleanPath(apppath
    + QDir::separator()
    + "db"
    + QDir::separator()
    + settings().host().replace(QRegExp("[:/]+"), "_")
    );
  
  p_->db.reset();
  p_->storage.reset(new storage_t(settings().localfolder(), prefix));
  p_->db.reset(new database::fs_t(*p_->storage, dbpath));

  if (!p_->db->initialized()) {
    p_->db->clear();
  }
  
  p_->enabled_a->setCheckable(true);
  p_->enabled_a->setChecked(settings().enabled());  

  QUrl url(settings().host());
  url.setPath(settings().remotefolder());
  
  p_->ui.remotefolder->setText(url.toString());
  p_->ui.localfolder->setText(settings().localfolder());
  
  manager_t::connection conn = {
    url.scheme(),
    url.host(),
    url.port(),
    settings().username(),
    settings().password(),
    url
  };
  
  p_->manager.reset(new manager_t(p_->db, conn));
  
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(new_actions(Actions)), this, SLOT(status_updated(Actions))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(error(QString)),       this, SLOT(set_error(QString)))); 

  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_started(action_t)), this, SLOT(action_started(action_t))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_success(action_t)), this, SLOT(action_success(action_t))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(action_error(action_t,QString)), this, SLOT(action_error(action_t,QString))));
  Q_VERIFY(connect(p_->manager.get(), SIGNAL(progress(action_t,qint64,qint64)), this, SLOT(action_progress(action_t,qint64,qint64))));

  Q_VERIFY(::connect(p_->manager.get(), SIGNAL(finished()), [this] {
    settings().set_last_sync(QDateTime::currentDateTime());
    
    if (p_->state != error) {
      p_->db->set_initialized(true);
      set_state(complete);
    }
  }));
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

void main_window_t::force_sync()
{
/*  
  QNetworkRequest request(url);
  
  QString concatenated = "kinnalru:malder22";
  QByteArray data = concatenated.toLocal8Bit().toBase64();
  QString headerData = "Basic " + data;
  request.setRawHeader("Authorization", headerData.toLocal8Bit());
  
//   QNetworkReply* reply = na->head(request);
  
  QNetworkReply* reply = na->get(request);
  
  QEventLoop loop;

  reply->ignoreSslErrors();
  
  connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
  
  loop.exec();
  
  qDebug() << "Error:" << reply->errorString();
  qDebug() << "Header1:" << reply->header(QNetworkRequest::ContentTypeHeader);
  qDebug() << "Header2:" << reply->header(QNetworkRequest::LastModifiedHeader);
  qDebug() << "RawHeader:" << reply->rawHeaderPairs();
  qDebug() << "data:" << reply->readAll();*/

   
  
  if (p_->manager.get() && p_->manager->busy()) return;
  
  set_state(syncing);
  p_->ui.actions->clear();
  p_->manager->start();
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
    
    auto unchanged = groupit(action_t::unchanged);
    unchanged = p_->ui.actions->takeTopLevelItem(p_->ui.actions->indexOfTopLevelItem(unchanged));
    p_->ui.actions->addTopLevelItem(unchanged);
    
    p_->ui.actions->resizeColumnToContents(0);
    p_->ui.actions->resizeColumnToContents(1);
    p_->ui.actions->resizeColumnToContents(2);
    
    p_->actions = actions;
}

void main_window_t::set_error(const QString& error)
{
    log(QString("Error: %1 ").arg(error));
//     const auto text = tr("Can't update status");
//     auto it = find_item(p_->ui.errors, text);
//     if (!it) {
//         QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << text << error);
//         p_->ui.errors->addTopLevelItem(item);
//     }
//     
    set_state(gui_state_e::error);
}

void main_window_t::action_started(const action_t& action)
{
  log(QString("Started %1: %2").arg(action.type_text()).arg(action.key), action);
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
  qDebug() << "123123123123";
  log(QString("Completed %1: %2").arg(action.type_text()).arg(action.key), action);
  auto it = find_item(p_->ui.actions, action.key);
  Q_ASSERT(it);
  it->setText(1, tr("Completed"));
  action_finished(action);    
}

void main_window_t::action_error(const action_t& action, const QString& message)
{
  log(QString("Error %1: %2(%3)").arg(action.type_text()).arg(action.key).arg(message), action);
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

void main_window_t::set_state(main_window_t::gui_state_e state)
{
  p_->state = state;
  
  switch(p_->state) {
  case waiting: {
    p_->ui.status->setPixmap(QPixmap("icons:state-pause.png"));
    p_->tray->setIcon(QIcon("icons:state-pause.png"));
    break;
  }
  case disabled: {
    p_->ui.status->setPixmap(QPixmap("icons:state-offline.png"));            
    p_->tray->setIcon(QIcon("icons:state-offline.png"));
    break;
  }
  case syncing: {
    p_->ui.status->setPixmap(QPixmap("icons:state-sync.png"));
    p_->tray->setIcon(QIcon("icons:state-sync.png"));        
    p_->sync_a->setEnabled(false);
    break;
  }
  case complete: {
    p_->ui.status->setPixmap(QPixmap("icons:state-ok.png"));
    p_->tray->setIcon(QIcon("icons:state-ok.png"));
    p_->sync_a->setEnabled(true);
    
    break;
  }
  case error: {
    p_->ui.status->setPixmap(QPixmap("icons:state-error.png"));            
    p_->tray->setIcon(QIcon("icons:state-error.png"));
    p_->sync_a->setEnabled(true);
    break;
  }
    default: Q_ASSERT(!"unhandled state");
  }
}

void main_window_t::log(const QString& message)
{
  p_->ui.log->insertRow(0);
  p_->ui.log->setItem(0, 0, new QTableWidgetItem(message));  
  p_->ui.log->setItem(0, 1, new QTableWidgetItem(QDateTime::currentDateTime().toString()));
  p_->ui.log->resizeColumnToContents(0);
}

void main_window_t::log(const QString& message, const action_t& action)
{
  if (action.type != action_t::unchanged) {
    log(message);
  }
}




