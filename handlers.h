#pragma once

#include <functional>

#include <QObject>

#include "session.h"
#include "sync.h"
#include "database/database.h"

struct base_handler_t {
    virtual void do_check(session_t& session, database_p& db, const action_t& action) const = 0;
    virtual Actions do_request(session_t& session, database_p& db, const action_t& action) const = 0;
    
    inline Actions operator() (session_t& session, database_p db, const action_t& action) const {
        try {
            do_check(session, db, action);
            return do_request(session, db, action);
        }
        catch (std::runtime_error& e) {
            throw qt_exception_t(QObject::tr("Action state error: ") + e.what());
        }
        return Actions();
    }
};

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
    
    action_processor_t(session_t& session, database_p db, Comparer comparer = Comparer(), Resolver resolver = Resolver());
    virtual ~action_processor_t(){};
    
    void process(const action_t& action);
  
Q_SIGNALS:
  void new_actions(const Actions&);

private:
    session_t& session_;
    database_p db_;
    std::map<action_t::type_e, std::function<Actions (session_t&, database_p db, const action_t&)>> handlers_;
};

