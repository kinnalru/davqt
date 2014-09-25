#pragma once

#include <memory>

#include <QObject>
#include <QRunnable>

#include "manager.h"
#include "session.h"

class syncer_t : public QObject, public QRunnable {
  Q_OBJECT

public:
  
  syncer_t(database_p db, const manager_t::connection& connection, manager_t& manager/*, QMutex* mx, Actions* actions*/);
  virtual ~syncer_t();
    
  virtual void run(); 
   
public Q_SLOTS:
  void stop();
  void new_actions_available();
  void can_finish();
    
Q_SIGNALS:
  void action_started(const action_t& action);
  void progress(const action_t& action, qint64 progress, qint64 total);  
  void action_success(const action_t& action);
  void action_error(const action_t& action, const QString& error);  

  void started();
  void error(const QString&);
  void finished();
  void stopping();
    
private:
 
    
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};
