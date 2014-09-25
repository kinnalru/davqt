#pragma once

#include <memory>

#include <QObject>
#include <QRunnable>

#include "manager.h"
#include "session.h"

class updater_t : public QObject, public QRunnable {
  Q_OBJECT

public:
  
  updater_t(database_p db, const manager_t::connection& connection);
  virtual ~updater_t();
    
  virtual void run(); 
  
public Q_SLOTS:
  void stop();
    
Q_SIGNALS:
  void new_actions(const Actions& actions);
  
  void started();
  void error(const QString& message);
  void finished();
  void stopping();
    
private:
  Actions update(session_t& session, const QString& folder);
  Actions process(QSet<QString> db, QSet<QString> local, QSet<QString> remote, session_t& s, QString folder = QString());
  Actions fill(Actions actions) const;
  
  action_t::type_e compare(const action_t& action) const;
  
    
private:
  struct Pimpl;
  std::unique_ptr<Pimpl> p_;
};
