#pragma once

#include <functional>

#include "session.h"
#include "sync.h"

class action_processor_t {
public:
    action_processor_t(session_t& session, db_t& db);
    
    void process(const action_t& action);

private:
    session_t& session_;
    db_t& db_;
    std::map<action_t::type_e, std::function<void (session_t&, db_t&, const action_t&)>> handlers_;
};

