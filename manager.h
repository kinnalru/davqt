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
  
  enum status_e {
    free,
    busy,
    stopping    
  };
  
  status_e status() const;
  const Actions& actions() const;
  
public Q_SLOTS:
  void start_update(); /// calculate tasks for sync
  void start_sync();   /// sync 
  void stop();         /// stop any activity

Q_SIGNALS:

  /// manager status change
  void status(status_e);
  void update_result(const Actions& actions);
  void sync_complete();
  
  void error(const QString);
  
  void action_started(const action_t& action);
  void progress(const action_t& action, qint64 progress, qint64 total);
  void action_success(const action_t& action);
  void action_error(const action_t& action, const QString& error);
  
  void need_stop();
  
private Q_SLOTS:
  void update_complete(const Actions& actions);
  void update_finished();
  void sync_finished();
  
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};