
#include <QThreadPool>
#include <QPointer>

#include "updater.h"
#include "syncer.h"

#include "tools.h"
#include "manager.h"



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
  
  QMutex mx;
  Actions actions;
  
  
  int active_updaters;
  int active_syncers;
};


manager_t::manager_t(database_p db, connection conn)
  : p_(new Pimpl)
{
  p_->db = db;
  p_->conn = conn;
  
  qDebug() << "[MANAGER]: using idealThreadCount:" << p_->thread_count;
}

manager_t::~manager_t()
{
  stop();
}

bool manager_t::busy() const
{
  return p_->active_updaters && p_->active_syncers;
}

void manager_t::start()
{
  start_update();
}

void manager_t::start_update()
{
  Q_ASSERT(!p_->active_updaters);
  
  auto updater = new updater_t(p_->db, p_->conn);
  updater->setAutoDelete(true);
  
  Q_VERIFY(::connect(updater, SIGNAL(started()), [this] {
    p_->active_updaters++;
    start_sync();
  }, Qt::BlockingQueuedConnection));
  
  Q_VERIFY(connect(updater, SIGNAL(new_actions(Actions)), SLOT(receive_new_actions(Actions))));
  Q_VERIFY(connect(updater, SIGNAL(error(QString)),       SIGNAL(error(QString))));
  
  Q_VERIFY(::connect(updater, SIGNAL(finished()), [this] {
    p_->active_updaters--;
    if (p_->active_updaters == 0)  Q_EMIT can_finish();
  }, Qt::BlockingQueuedConnection));
  
  Q_VERIFY(connect(this, SIGNAL(need_stop()), updater,    SLOT(stop())));
  
  QThreadPool::globalInstance()->start(updater);
}

void manager_t::receive_new_actions(const Actions& actions)
{
  Q_ASSERT(p_->active_updaters);
  Q_EMIT new_actions(actions);  
  p_->actions << actions;
}

void manager_t::start_sync()
{
  Q_ASSERT(p_->active_updaters);
  
  for (int i = 0; i < (p_->thread_count - 1) ; ++i) {
    auto syncer = new syncer_t(p_->db, p_->conn, *this);
    syncer->setAutoDelete(true);
    
    Q_VERIFY(::connect(syncer, SIGNAL(started()), [this] {
      p_->active_syncers++;
    }, Qt::BlockingQueuedConnection));
    
    Q_VERIFY(connect(syncer, SIGNAL(action_started(action_t)),        SIGNAL(action_started(action_t))));
    Q_VERIFY(connect(syncer, SIGNAL(progress(action_t,qint64,qint64)),SIGNAL(progress(action_t,qint64,qint64))));
    Q_VERIFY(connect(syncer, SIGNAL(action_success(action_t)),        SIGNAL(action_success(action_t))));
    Q_VERIFY(connect(syncer, SIGNAL(action_error(action_t,QString)),  SIGNAL(action_error(action_t,QString))));
    Q_VERIFY(connect(syncer, SIGNAL(error(QString)),                  SIGNAL(error(QString))));
    
    Q_VERIFY(::connect(syncer, SIGNAL(finished()), [this] {
      p_->active_syncers--;
      if (p_->active_syncers == 0) Q_EMIT finished();
    }, Qt::BlockingQueuedConnection));
    
    Q_VERIFY(connect(this, SIGNAL(new_actions(Actions)), syncer,      SLOT(new_actions_available())));
    Q_VERIFY(connect(this, SIGNAL(need_stop()), syncer,               SLOT(stop())));
    Q_VERIFY(connect(this, SIGNAL(can_finish()), syncer,              SLOT(can_finish())));
    
    QThreadPool::globalInstance()->start(syncer);
  }
}

void manager_t::stop()
{
  Q_EMIT need_stop();
  QThreadPool::globalInstance()->waitForDone();
}


action_t manager_t::next_sync_action()
{
  QMutexLocker lock(&p_->mx);
  if (!p_->actions.isEmpty()) {
    return p_->actions.takeLast();
  }
  else {
    return action_t();
  }
}


