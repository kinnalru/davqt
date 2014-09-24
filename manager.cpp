
#include <QThreadPool>

#include "updater.h"
#include "syncer.h"

#include "tools.h"
#include "manager.h"



struct manager_t::Pimpl {
  Pimpl() : status(free) {};
  
  database_p db;
  connection conn;
  status_e status;
  
  QMutex mx;
  Actions actions;
};


manager_t::manager_t(database_p db, connection conn)
  : p_(new Pimpl)
{
  p_->db = db;
  p_->conn = conn;
}

manager_t::~manager_t()
{
}

void manager_t::start_update()
{
  Q_ASSERT(p_->status == free);
  p_->status = busy;
  
  auto updater = new updater_t(p_->db, p_->conn);
  updater->setAutoDelete(true);
  
  Q_VERIFY(connect(updater, SIGNAL(status(Actions)), SLOT(update_result(Actions))));
  Q_VERIFY(connect(updater, SIGNAL(error(QString)), SIGNAL(error(QString))));
  Q_VERIFY(connect(updater, SIGNAL(finished()), SLOT(update_complete())));
  Q_VERIFY(connect(this, SIGNAL(need_stop()), updater, SLOT(stop())));
  
  QThreadPool::globalInstance()->start(updater);
}

void manager_t::update_result(const Actions& actions)
{
  Q_ASSERT(p_->status == busy);
  p_->actions = actions;
}

void manager_t::update_complete()
{
  Q_ASSERT((p_->status == busy) || (p_->status == stopping));
  p_->status = free;
  Q_EMIT update_finished(p_->actions);  
}



void manager_t::start_sync()
{
  Q_ASSERT(p_->status == free);
  p_->status = busy;
  
  for (int i = 0; i < 1; ++i) {
    auto syncer = new syncer_t(p_->db, p_->conn, *this/*, &p_->mx, &p_->actions*/);
    syncer->setAutoDelete(true);
    
    Q_VERIFY(connect(syncer, SIGNAL(action_started(action_t)), SIGNAL(action_started(action_t))));
    Q_VERIFY(connect(syncer, SIGNAL(progress(action_t,qint64,qint64)), SIGNAL(progress(action_t,qint64,qint64))));
    Q_VERIFY(connect(syncer, SIGNAL(action_success(action_t)), SIGNAL(action_success(action_t))));
    Q_VERIFY(connect(syncer, SIGNAL(action_error(action_t,QString)), SIGNAL(action_error(action_t,QString))));
    Q_VERIFY(connect(syncer, SIGNAL(error(QString)), SIGNAL(error(QString))));
    Q_VERIFY(connect(syncer, SIGNAL(finished()), SLOT(sync_complete())));
    Q_VERIFY(connect(this, SIGNAL(need_stop()), syncer, SLOT(stop())));
    
    QThreadPool::globalInstance()->start(syncer);
  }
}

void manager_t::sync_complete()
{
  Q_ASSERT((p_->status == busy) || (p_->status == stopping));
  p_->status = free;
  Q_EMIT sync_finished();
}

void manager_t::stop()
{
  Q_ASSERT(p_->status == busy);
  p_->status = stopping;
  Q_EMIT need_stop();
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

manager_t::status_e manager_t::status() const
{
  return p_->status;
}




