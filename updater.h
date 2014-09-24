#pragma once

#include <memory>

#include <QObject>

#include "database/database.h"
#include "session.h"
#include "types.h"

class updater_t : public QObject {
  Q_OBJECT

public:
  
  updater_t(database_p db, const connection_t& connection);
  virtual ~updater_t();
    
   
public Q_SLOTS:
  void start();
  void stop();
    
Q_SIGNALS:
  
  void started();  
  void finished();
  
  void status(const Actions& actions);
  
  void stopping();
    
private:
  QList<action_t> update(session_t& session, const QString& lf, const QString& rf);
  QList<action_t> process(QSet<QString> db, QSet<QString> local, QSet<QString> remote, session_t& s, QString folder = QString());
  QList<action_t> fill(QList<action_t> actions) const;
  
  action_t::type_e compare(const action_t& action) const;
  
    
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};
