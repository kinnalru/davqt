#pragma once

#include <memory>

#include <QObject>

#include "database/database.h"
#include "session.h"
#include "types.h"

class syncer_t : public QObject {
  Q_OBJECT

public:
  
  syncer_t(database_p db, const connection_t& connection, const QList<action_t>& actions);
  virtual ~syncer_t();
    
   
public Q_SLOTS:
  void start();
  void stop();
    
Q_SIGNALS:
  
  void started();  
  void finished();
  
  void stopping();
    
private:
 
    
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};
