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
  
  handler_t(database_p& d, manager_t& m, const action_t& a) : db(d), manager(m), action(a)
  {}

  Q_SLOT virtual void stop() {};
  
Q_SIGNALS:
  void started(const action_t& action) const;
  void finished(const action_t& action) const;
  
  void progress(const action_t& action, qint64 progress, qint64 total) const;
  void error(const action_t& action, QString messabe) const;
  
  void new_actions(const Actions& actions) const;
  
  ///Use only Qt::BlockingQueuedConnection for this signal
  void compare(conflict_ctx& ctx, bool& result) const;
  ///Use only Qt::BlockingQueuedConnection for this signal
  void merge(conflict_ctx& ctx, bool& result) const;
  
  void need_stop();
  
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

