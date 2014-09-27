#pragma once

#include <memory>

#include <QObject>
#include <QUrl>

#include "database/database.h"
#include "types.h"

class handler_t;

class manager_t : public QObject {
  Q_OBJECT
  Q_ENUMS(status_e)
public:
  
  struct connection {
    QString schema;
    QString host;
    quint32 port;
    QString login;
    QString password;
    QUrl    url;
  };
  
  explicit manager_t(database_p db, connection conn);
  virtual ~manager_t();
  
  bool busy() const;
  
  QWebdav* network() const;
  
public Q_SLOTS:
  void start();  /// start updating and sync process
  void stop();   /// stop any activity
  void wait();

Q_SIGNALS:

  void finished();
  
  void new_actions(const Actions& actions);
  void error(const QString);
  
  void action_started(const action_t& action);
  void progress(const action_t& action, qint64 progress, qint64 total);
  void action_success(const action_t& action);
  void action_error(const action_t& action, const QString& error);
  
  void need_stop();
  void can_finish();
  
private Q_SLOTS:
  void receive_new_actions(const Actions& actions);
  
  void compare(conflict_ctx& ctx, bool& result);
  void merge(conflict_ctx& ctx, bool& result);

private:
  void start_update(); /// calculate tasks for sync
  handler_t* create_handler(const action_t& action);
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};






