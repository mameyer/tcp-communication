#include "Subject.hpp"

void
Subject::attach(TCPObserver *observer)
{
    this->observers.push_back(observer);
}

void
Subject::detach(TCPObserver *observer)
{
    this->observers.remove(observer);
}

void
Subject::notifyObserver(void (TCPObserver::*func)())
{
    std::list<TCPObserver *>::iterator it;
    
    for (it = this->observers.begin(); it != this->observers.end(); it++) {
        ((*it)->*func)();
    }
}