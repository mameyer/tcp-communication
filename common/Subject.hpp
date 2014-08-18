#ifndef SUBJECT_HPP__
#define SUBJECT_HPP__

#include "Observer.hpp"

#include <list>

class Subject {
public:
    std::list<Observer *> observers;
    
    virtual void attach(Observer *observer) = 0;
    virtual void detach(Observer *observer) = 0;
    virtual void notifyObserver(void (Observer::*func)()) = 0;
};

#endif