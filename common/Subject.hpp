#ifndef SUBJECT_HPP__
#define SUBJECT_HPP__

#include "TCPObserver.hpp"

#include <list>

class Subject {
public:
    std::list<TCPObserver *> observers;
    
    virtual void attach(TCPObserver *observer);
    virtual void detach(TCPObserver *observer);
    virtual void notifyObserver(void (TCPObserver::*func)());
};

#endif