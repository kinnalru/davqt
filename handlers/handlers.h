#pragma once

#include <functional>

#include <QObject>

#include "manager.h"
#include "session.h"
#include "sync.h"
#include "database/database.h"

class manager_t;
class QWebdav;

class handler_t : public QObject, public QRunnable {
  Q_OBJECT
 
  typedef std::function<QNetworkReply* (QWebdav*)> synchro_reply_f;
 
public:
  
  struct conflict_ctx {
    const action_t action;
    QString local_file;
    QString remote_file;
    QString result_file;
  };
  
  handler_t(database_p& d, manager_t& m, const action_t& a) : db(d), manager(m), action(a)
  {}

  Q_SLOT virtual void stop() {};
  
Q_SIGNALS:
  void started(const action_t& action) const;
  void finished(const action_t& action) const;
  
  void progress(const action_t& action, qint64 progress, qint64 total) const;
  void error(const action_t& action, QString messabe) const;
  
  void new_actions(const Actions& actions) const;
  
  ///Use only Qt::BlockingQueuedConnection for this sugnal
  void compare(conflict_ctx& ctx, bool& result);
  
protected:
  virtual void do_check() const = 0;
  virtual void do_request() const = 0;
  
  virtual void  run();

  template <typename P>
  auto deliver(P package) const -> decltype(package(0)) {
    decltype(package(0)) result;
    new Package(&manager, [&, this] () {
      result = package(manager.network());
    },
    Qt::BlockingQueuedConnection);
    return result;
  }
  
  QNetworkReply* deliver_and_block(synchro_reply_f package) const;

  
protected:
  database_p db;
  const action_t action;

private:
  manager_t& manager;
};

struct upload_handler : handler_t {
  
  upload_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}
  
  void do_check() const override;
  void do_request() const override;
};

struct download_handler : handler_t {
  
  download_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}
  
  void do_check() const override;
  void do_request() const override;
};

struct forgot_handler : handler_t {
  
  forgot_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}

  void do_check() const override;
  void do_request() const override;
};

struct local_change_handler : upload_handler {
  
  local_change_handler(database_p& d, manager_t& m, const action_t& a) : upload_handler(d, m, a){}

  void do_check() const override;
  void do_request() const override;
};

struct remote_change_handler : download_handler {
  
  remote_change_handler(database_p& d, manager_t& m, const action_t& a) : download_handler(d, m, a){}

  void do_check() const override;
};

struct conflict_handler : handler_t {
//   action_processor_t::Resolver resolver_;
//   action_processor_t::Comparer comparer_;
  
  conflict_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}

  void do_check() const override;
  void do_request() const override;
};

struct remove_local_handler : handler_t {
  
  remove_local_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}
  
  void do_check() const override;
  void do_request() const override;
  
private:
  bool remove_dir(const QString & path) const;
};

struct remove_remote_handler : handler_t {
  
  remove_remote_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}

  void do_check() const override;
  void do_request() const override;
};

struct upload_dir_handler : handler_t {
  
  upload_dir_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}

  void do_check() const override;  
  void do_request() const override;
};

struct download_dir_handler : handler_t {
  
  download_dir_handler(database_p& d, manager_t& m, const action_t& a) : handler_t(d, m, a){}
  
  void do_check() const override;    
  void do_request() const override;
};
/*
QRunnable* get_handler(database_p db, manager_t& manager, const action_t& action)
{
  switch(action.type) {
    case action_t::upload:         return new upload_handler(db, manager, action);
    case action_t::download:       return new download_handler(db, manager, action);
    case action_t::forgot:         return new forgot_handler(db, manager, action);
    case action_t::remove_local:   return new remove_local_handler(db, manager, action);
    case action_t::remove_remote:  return new remove_remote_handler(db, manager, action);
    case action_t::local_changed:  return new local_change_handler(db, manager, action);
    case action_t::remote_changed: return new remote_change_handler(db, manager, action);

//     case action_t::upload_dir,   upload_dir_handler()},
//     case action_t::download_dir, download_dir_handler()},
//     case {action_t::unchanged, [] (QWebdav& webdav, database_p db, const action_t& action) {return Actions();}},
//     case action_t::conflict, conflict_handler(comparer, resolver)},
  };
}*/

/*
action_processor_t::action_processor_t(database_p db, manager_t& manager, action_processor_t::Comparer comparer, action_processor_t::Resolver resolver)
    : manager(manager)
    , db_(db)
{
    
    handlers_ = {*/
//         {action_t::upload,   upload_handler()},
//         {action_t::download, download_handler()},
//         {action_t::forgot,   forgot_handler()},
//         {action_t::remove_local,   remove_local_handler()},
//         {action_t::remove_remote,  remove_remote_handler()},
//         {action_t::local_changed,  local_change_handler()},
//         {action_t::remote_changed, remote_change_handler()},
//         
//         {action_t::upload_dir,   upload_dir_handler()},
//         {action_t::download_dir, download_dir_handler()},
//         {action_t::unchanged, [] (QWebdav& webdav, database_p db, const action_t& action) {return Actions();}},
//         {action_t::conflict, conflict_handler(comparer, resolver)},
//         {action_t::error, [this] (QWebdav& webdav, database_p db, const action_t& action) {
//             throw qt_exception_t(QObject::tr("Action precondition failed"));
//             return Actions();
//         }}
//     };
// }

// void action_processor_t::process(const action_t& action)
// {
//     auto h = handlers_.find(action.type);
//     if (h != handlers_.end()) {
//         debug() << QString("processing action: %1 [%2]").arg(action.type_text()).arg(action.key);
// //         Actions actions = h->second(webdav_, db_, action);
// //         if (!actions.isEmpty()) {
// //           Q_EMIT new_actions(actions);
// //         }
//         debug() << "completed";
//     }
//     else {
//         qCritical() << "unhandled action type!:" << action;
//         Q_ASSERT(!"unhandled action type!");
//     }
// }
/*
class action_processor_t : public QObject {
  Q_OBJECT
public:
    struct resolve_ctx {
        const action_t& action;
        QString local_old;
        QString remote_new;
        QString result;
    };

    typedef std::function<bool (resolve_ctx&)> Comparer; //used to compare files for equality    
    typedef std::function<bool (resolve_ctx&)> Resolver; //used to merge conflict files
    
    action_processor_t(database_p db, manager_t& manager, Comparer comparer = Comparer(), Resolver resolver = Resolver());
    virtual ~action_processor_t(){};
    
    void process(const action_t& action);
  
Q_SIGNALS:
  void new_actions(const Actions&);

private:
    database_p db_;
    manager_t& manager;
    std::map<action_t::type_e, std::function<Actions (QWebdav&, database_p db, const action_t&)>> handlers_;
};*/

