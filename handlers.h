#pragma once

#include <functional>

#include "session.h"
#include "sync.h"
#include "database.h"

struct base_handler_t {
    virtual bool do_check(session_t& session, const action_t& action) const = 0;
    virtual void do_request(session_t& session, db_t& db, const action_t& action) const = 0;
    
    inline void operator() (session_t& session, db_t& db, const action_t& action) const {
        if (do_check(session, action)) {
            do_request(session, db, action);
        } 
        else {
            throw qt_exception_t(QObject::tr("Action state error"));
        }
    }
};

class action_processor_t {
public:
    struct resolve_ctx {
        const action_t& action;
        QString local_old;
        QString remote_new;
        QString result;
    };
    
    typedef std::function<bool (resolve_ctx&)> Resolver;
    action_processor_t(session_t& session, db_t& db, Resolver resolver);
    
    bool process(const action_t& action);

private:
    session_t& session_;
    db_t& db_;
    std::map<action_t::type_e, std::function<void (session_t&, db_t&, const action_t&)>> handlers_;
};

