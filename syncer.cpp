
#include "handlers.h"
#include "tools.h"
#include "syncer.h"

struct stop_exception_t : public std::runtime_error {
  stop_exception_t() : std::runtime_error("stop") {}
};

struct syncer_t::Pimpl {
  Pimpl(manager_t& m) : manager(m) {}
  
  database_p db;
  manager_t::connection conn;
  manager_t& manager;
  bool stop;
  
//   QMutex* mx;
//   Actions* actions;
};

syncer_t::syncer_t(database_p db, const manager_t::connection& connection, manager_t& manager)
  : QObject(0)
  , p_(new Pimpl(manager))
{
  p_->db = db;
  p_->conn = connection;
  p_->stop = false;
  
//   Q_ASSERT(mx);
//   Q_ASSERT(actions);
//   
//   p_->mx = mx;
//   p_->actions = actions;
}

syncer_t::~syncer_t()
{
}


void syncer_t::run()
{
  qDebug() << "Sync thread started";  
  try {
    
//     const auto next_action = [this] () {
//       action_t current;
//       QMutexLocker locker(p_->mx);
//       
//       if (!p_->actions->isEmpty()) {
//           current = p_->actions->takeFirst();
//       }
//       return current;
//     };
    
    session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
              
    if (p_->stop) throw stop_exception_t();
    
    Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));
    
    session.set_auth(p_->conn.login, p_->conn.password);
    session.set_ssl();
    session.open();
    
    if (p_->stop) throw stop_exception_t();
    
    progress_adapter_t adapter;

    Q_VERIFY(connect(&session, SIGNAL(get_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
    Q_VERIFY(connect(&session, SIGNAL(put_progress(qint64,qint64)), &adapter, SLOT(int_progress(qint64,qint64))));
    Q_VERIFY(connect(&adapter, SIGNAL(progress(action_t,qint64,qint64)), this, SIGNAL(progress(action_t,qint64,qint64))));
    
    action_processor_t processor(session, p_->db);
    //     action_processor_t processor(session, p_->db,
    //         [] (action_processor_t::resolve_ctx& ctx) {
    //             return !QProcess::execute(QString("diff"), QStringList() << ctx.local_old << ctx.remote_new);
    //         },
    //         [] (action_processor_t::resolve_ctx& ctx) {
    //             ctx.result = ctx.local_old + ".merged" + db_t::tmpprefix;
    //             return !QProcess::execute(QString("kdiff3"), QStringList() << "-o" << ctx.result << ctx.local_old << ctx.remote_new);
    //         });
    
    Q_FOREVER {
//       action_t current = next_action();
      action_t current;
      if (!QMetaObject::invokeMethod(&p_->manager, "next_sync_action", Qt::BlockingQueuedConnection, Q_RETURN_ARG(action_t, current))) {
          throw std::runtime_error("Can't dequeue next action to sync");
      }
      
      if (current.empty()) break;
      
      adapter.set_action(current);
      
      try {
        Q_EMIT action_started(current);      
        processor.process(current);
        Q_EMIT action_success(current);
      }
      catch(const std::exception& e) {
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
}

