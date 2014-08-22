#ifndef OBSERVER_HPP__
#define OBSERVER_HPP__

class TCPObserver {
public:
    virtual void update_messages() {};
    virtual void update_income() {};
    virtual void add_conn() {};
    virtual void rm_conn() {};
    virtual void register_server_answer() {};
    virtual void handle_server_disconnect() {};
};

#endif