
#include <QThreadPool>
#include <QPointer>
#include <QProcess>

#include "updater.h"
#include "syncer.h"

#include "tools.h"
#include "manager.h"
#include "handlers/handlers.h"
#include "settings/settings.h"


struct manager_t::Pimpl {
  Pimpl()
    : thread_count(std::max(QThread::idealThreadCount(), 2))
    , active_updaters(0)
    , active_syncers(0)
  {
    QThreadPool::globalInstance()->setMaxThreadCount(thread_count);
  };
  
  database_p db;
  connection conn;
  const int thread_count;
  
  std::unique_ptr<QWebdav> network;
  
  int active_updaters;
  int active_syncers;
};


manager_t::manager_t(database_p db, connection conn)
  : p_(new Pimpl)
{
  p_->db = db;
  p_->conn = conn;
  
  p_->network.reset(new QWebdav(conn.url, 0, QWebdav::Async));
  
  qDebug() << "[MANAGER]: using idealThreadCount:" << p_->thread_count;
}

manager_t::~manager_t()
{
  stop();
}

bool manager_t::busy() const
{
  return p_->active_updaters || p_->active_syncers;
}

QWebdav* manager_t::network() const
{
  return p_->network.get();
}


void manager_t::start()
{
  start_update();
}

void manager_t::start_update()
{
  Q_ASSERT(!p_->active_updaters);
  
  p_->active_updaters++;
  auto updater = new updater_t(p_->db, p_->conn);
  updater->setAutoDelete(true);
  
  Q_VERIFY(connect(updater, SIGNAL(new_actions(Actions)), SLOT(receive_new_actions(Actions))));
  Q_VERIFY(connect(updater, SIGNAL(error(QString)),       SLOT(stop())));
  Q_VERIFY(connect(updater, SIGNAL(error(QString)),       SIGNAL(error(QString))));
  
  Q_VERIFY(::connect(updater, SIGNAL(finished()), [this] {
    p_->active_updaters--;
    if (p_->active_syncers == 0 && p_->active_updaters == 0) {
      Q_EMIT finished();
    }
  }, Qt::BlockingQueuedConnection));
  
  Q_VERIFY(connect(this, SIGNAL(need_stop()), updater,    SLOT(stop())));
  
  QThreadPool::globalInstance()->start(updater);
}

void manager_t::receive_new_actions(const Actions& actions)
{
  Q_ASSERT(busy());
  Q_EMIT new_actions(actions);  
  
  Q_FOREACH(auto action, actions) {
    handler_t* handler = create_handler(action);
    
    if (!handler) continue;
    
    p_->active_syncers++;
    
    Q_VERIFY(connect(handler, SIGNAL(new_actions(Actions)),            SLOT(receive_new_actions(Actions))));
    Q_VERIFY(connect(handler, SIGNAL(started(action_t)),               SIGNAL(action_started(action_t))));
    Q_VERIFY(connect(handler, SIGNAL(progress(action_t,qint64,qint64)),SIGNAL(progress(action_t,qint64,qint64))));
    Q_VERIFY(connect(handler, SIGNAL(finished(action_t)),              SIGNAL(action_success(action_t))));
    Q_VERIFY(connect(handler, SIGNAL(error(action_t, QString)),        SIGNAL(action_error(action_t,QString))));
    Q_VERIFY(connect(handler, SIGNAL(error(action_t, QString)),        SLOT(stop())));
    
    Q_VERIFY(connect(handler, SIGNAL(compare(conflict_ctx&,bool&)),    SLOT(compare(conflict_ctx&,bool&)), Qt::BlockingQueuedConnection));
    Q_VERIFY(connect(handler, SIGNAL(merge(conflict_ctx&,bool&)),      SLOT(merge(conflict_ctx&,bool&)), Qt::BlockingQueuedConnection));
    
    Q_VERIFY(connect(this, SIGNAL(need_stop()), handler,               SLOT(stop())));    
    
    Q_VERIFY(::connect(handler, SIGNAL(finished(action_t)), [this] {
      p_->active_syncers--;
      if (p_->active_syncers == 0 && p_->active_updaters == 0) {
        Q_EMIT finished();
      }
    }, Qt::BlockingQueuedConnection));

    handler->setAutoDelete(true);
    QThreadPool::globalInstance()->start(handler);
  }
}

void manager_t::compare(conflict_ctx& ctx, bool& result)
{
  int code = QProcess::execute(settings().compare(), QStringList() << ctx.local_file << ctx.remote_file);
  result = !code;
  qDebug() << "merge result:" << code;
}


void manager_t::merge(conflict_ctx& ctx, bool& result)
{
  ctx.result_file = ctx.local_file + ".merged" + storage_t::tmpsuffix;
  result = !QProcess::execute(settings().merge(), QStringList()
    << "-o"
    << ctx.result_file
    << ctx.local_file
    << ctx.remote_file);
}

handler_t* manager_t::create_handler(const action_t& action)
{
  switch(action.type) {
    case action_t::upload:         return new upload_handler(p_->db, *this, action);
    case action_t::download:       return new download_handler(p_->db, *this, action);
    case action_t::forgot:         return new forgot_handler(p_->db, *this, action);
    case action_t::remove_local:   return new remove_local_handler(p_->db, *this, action);
    case action_t::remove_remote:  return new remove_remote_handler(p_->db, *this, action);
    case action_t::local_changed:  return new local_change_handler(p_->db, *this, action);
    case action_t::remote_changed: return new remote_change_handler(p_->db, *this, action);
    
    case action_t::upload_dir:    return new upload_dir_handler(p_->db, *this, action);
    case action_t::download_dir:  return new download_dir_handler(p_->db, *this, action);
    case action_t::conflict:      return new conflict_handler(p_->db, *this, action);
    
    case action_t::error: throw qt_exception_t("Error action");
  };
  
  return 0;
}


void manager_t::stop()
{
  Q_EMIT need_stop();
}

void manager_t::wait()
{
  QThreadPool::globalInstance()->waitForDone();
}

