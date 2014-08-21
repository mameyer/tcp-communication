#ifndef OBSERVER_HPP__
#define OBSERVER_HPP__

class Observer {
public:
    virtual void update_messages() {};
    virtual void update_income() {};
    virtual void add_conn() {};
    virtual void rm_conn() {};
};

#endif