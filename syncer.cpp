
#include <QWaitCondition>
#include <QProcess>

#include "handlers.h"
#include "tools.h"
#include "syncer.h"

#include "3rdparty/webdav/qtwebdav/webdav_url_info.h"

struct stop_exception_t : public std::runtime_error {
  stop_exception_t() : std::runtime_error("stop") {}
};

struct syncer_t::Pimpl {
  Pimpl(manager_t& m) : manager(m) {}
  
  database_p db;
  manager_t::connection conn;
  manager_t& manager;
  bool stop;
  bool finish;
  
  QMutex mx;
  QWaitCondition available;
};

syncer_t::syncer_t(database_p db, const manager_t::connection& connection, manager_t& manager)
  : QObject(0)
  , p_(new Pimpl(manager))
{
  p_->db = db;
  p_->conn = connection;
  p_->stop = false;
  p_->finish = false;
}

syncer_t::~syncer_t()
{
}


void syncer_t::run()
{
  qDebug() << "Sync thread started";  
  Q_EMIT started();
  try {
    
    QWebdav webdav(p_->conn.url);
              
    if (p_->stop) throw stop_exception_t();
    
//     Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));
//     
//     session.set_auth(p_->conn.login, p_->conn.password);
//     session.set_ssl();
//     session.open();
    
    if (p_->stop) throw stop_exception_t();
    
    progress_adapter_t adapter;

//     Q_VERIFY(connect(&session, SIGNAL(get_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
//     Q_VERIFY(connect(&session, SIGNAL(put_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
//     Q_VERIFY(connect(&adapter, SIGNAL(progress(action_t,qint64,qint64)), this, SIGNAL(progress(action_t,qint64,qint64))));
    
    action_processor_t processor(webdav, p_->db,
        [] (action_processor_t::resolve_ctx& ctx) {
            return !QProcess::execute(QString("diff"), QStringList() << ctx.local_old << ctx.remote_new);
        },
        [] (action_processor_t::resolve_ctx& ctx) {
            ctx.result = ctx.local_old + ".merged" + storage_t::tmpsuffix;
            return !QProcess::execute(QString("kdiff3"), QStringList() << "-o" << ctx.result << ctx.local_old << ctx.remote_new);
        });
    
    Q_VERIFY(connect(&processor, SIGNAL(new_actions(Actions)), this, SIGNAL(new_actions(Actions))));
    
    Q_FOREVER {
      action_t current;
      if (!QMetaObject::invokeMethod(&p_->manager, "next_sync_action", Qt::BlockingQueuedConnection, Q_RETURN_ARG(action_t, current))) {
          throw std::runtime_error("Can't dequeue next action to sync");
      }
      if (p_->stop) throw stop_exception_t();
      
      if (current.empty()) {
        if (p_->finish) break;
        p_->available.wait(&p_->mx);
        if (p_->finish) break;
        if (p_->stop) throw stop_exception_t();
        continue;
      }
      
      adapter.set_action(current);
      
      try {
        Q_EMIT action_started(current);      
        processor.process(current);
        if (p_->stop) throw stop_exception_t();       
        Q_EMIT action_success(current);
      }
      catch (const stop_exception_t& e) {
        throw;
      }      
      catch(const std::exception& e) {
        if (p_->stop) throw stop_exception_t();   
        Q_EMIT action_error(current, e.what());
      }
    }

  }
  catch (const stop_exception_t& e) {
    qDebug() << "Sync thread force to stop";
  }
  catch (const std::exception& e) {
    qDebug() << "Sync thread exception:" << e.what();
    Q_EMIT error(e.what());
  }

  qDebug() << "Sync thread finished";
  Q_EMIT finished();
}

void syncer_t::stop()
{
  p_->stop = true;
  Q_EMIT stopping();
  p_->available.wakeAll();
}

void syncer_t::new_actions_available()
{
  p_->available.wakeAll();
}

void syncer_t::can_finish()
{
  p_->finish = true;
  p_->available.wakeAll();
}



