
#include "handlers.h"
#include "tools.h"
#include "syncer.h"

struct syncer_t::Pimpl {
  database_p db;
  connection_t conn;
  QList<action_t> actions;
  bool stop;
};

syncer_t::syncer_t(database_p db, const connection_t& connection, const QList<action_t>& actions)
  : QObject(0)
  , p_(new Pimpl)
{
  p_->db = db;
  p_->conn = connection;
  p_->actions = actions;
  p_->stop = false;
}

syncer_t::~syncer_t()
{
}


void syncer_t::start()
{
  Q_EMIT started();
  
  Q_FOREACH(const auto& action, p_->actions ) {
    
    session_t session(0, p_->conn.schema, p_->conn.host, p_->conn.port);
            
    if (p_->stop) {
      Q_EMIT finished();
      return;
    }
    Q_VERIFY(connect(this, SIGNAL(stopping()), &session, SLOT(cancell()), Qt::DirectConnection));

    session.set_auth(p_->conn.login, p_->conn.password);
    session.set_ssl();
    session.open();

    
//     action_processor_t processor(session, p_->db,
//         [] (action_processor_t::resolve_ctx& ctx) {
//             return !QProcess::execute(QString("diff"), QStringList() << ctx.local_old << ctx.remote_new);
//         },
//         [] (action_processor_t::resolve_ctx& ctx) {
//             ctx.result = ctx.local_old + ".merged" + db_t::tmpprefix;
//             return !QProcess::execute(QString("kdiff3"), QStringList() << "-o" << ctx.result << ctx.local_old << ctx.remote_new);
//         });
    
    
    qDebug() << "Preparing action processor...";
    action_processor_t processor(session, p_->db);

    try {
    
      qDebug() << "Process action";
      processor.process(action);
    }
    catch(std::exception& e) {
      qDebug() << "Exception:" << e.what();
    }
  }
 
  Q_EMIT finished();
}

void syncer_t::stop()
{

}

