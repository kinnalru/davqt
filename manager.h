#pragma once

#include <memory>

#include <QObject>

#include "database/database.h"
#include "types.h"

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
  };
  
  explicit manager_t(database_p db, connection conn);
  virtual ~manager_t();
  
  bool busy() const;
  
//   enum status_e {
//     free      = 1 << 1,
//     updating  = 1 << 2,
//     syncing   = 1 << 3,
//     stopping  = 1 << 4,
//     
//     busy_upd  = updating | stopping,
//     busy_sync = syncing  | stopping,
//     
//     busy      = busy_upd | busy_sync,
//   };
//   
//   status_e status() const;
  
public Q_SLOTS:
  void start();  /// start updating and sync process
  void stop();   /// stop any activity

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

private:
  void start_update(); /// calculate tasks for sync
  void start_sync();   /// sync   
  Q_INVOKABLE action_t next_sync_action();
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};