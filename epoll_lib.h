/*event library based on epoll*/
#ifndef EPOLL_TEST_H
#define EPOLL_TEST_H

#include <unistd.h>
#include <sys/epoll.h>
#include <map>
#include <iostream>

#define MAXEVENTS 64
#define SOCKET 1
#define LISTEN 2
#define ACCEPT 3

class EpollEvent;

struct Event{
    int fd;
    unsigned int FLAG;
    unsigned int event;
    void* (*callback)(int, EpollEvent&, void *arg);
    void *arg;
    Event():arg(NULL), callback(NULL){}
};

class IEvent{
public:
    virtual int addEvent(const Event &event) = 0;
    virtual int delEvent(const Event &event) = 0;
    virtual int dispatcher() = 0;

    virtual ~IEvent(){}    
};

class EpollEvent:public IEvent{
    public:
        EpollEvent(){
            if(initEvent()<0)
                abort();
        }

        virtual int addEvent(const Event &event);
        virtual int delEvent(const Event &event);
        virtual int dispatcher();

        ~EpollEvent(){}
    private:
        int initEvent(){
            int epollFd = epoll_create1(EPOLL_CLOEXEC);
            if(epollFd<0){
                std::cerr<<"create error:";
                return epollFd;
            }

            this->epollFd = epollFd;
            return 0;
        }
    private:
        int epollFd;

        std::map<int, Event> events;
};

int EpollEvent::addEvent(const Event &event){
    struct epoll_event epollEvent;

    epollEvent.data.fd = event.fd;
    epollEvent.events = event.event;
    int retCode = epoll_ctl(this->epollFd, EPOLL_CTL_ADD, event.fd, &epollEvent);
    if(retCode<0){
        std::cerr<<"epoll_ctl error:";
        return retCode;
    }

    this->events[event.fd] = event;
    return 0;
}

int EpollEvent::delEvent(const Event &event){
    struct epoll_event epollEvent;

    epollEvent.data.fd = event.fd;
    epollEvent.events = event.event;
    int retCode = epoll_ctl(this->epollFd, EPOLL_CTL_DEL, event.fd, &epollEvent);
    if(retCode<0){
        std::cerr<<"epoll_ctl del error:";
        return retCode;
    }

    this->events.erase(event.fd);
    return 0;
}

int EpollEvent::dispatcher(){
    struct epoll_event epollEvents[MAXEVENTS];

    int nEvents = epoll_wait(epollFd, epollEvents, MAXEVENTS, -1);
    if(nEvents<0){
        std::cerr<<"epoll_wait error:";
        return -1;
    }

    for(int i=0;i<nEvents;++i){
        int fd=epollEvents[i].data.fd;
        Event event = this->events[fd];
        if ((epollEvents[i].events & EPOLLERR) ||
            (epollEvents[i].events & EPOLLHUP) ||
            (!(epollEvents[i].events & EPOLLIN)))
        {
            /* An error has occured on this fd, or the socket is not
               ready for reading (why were we notified then?) */
            fprintf (stderr, "epoll error\n");
            close (epollEvents[i].data.fd);
            continue;
        }
        else if(event.FLAG==LISTEN && event.callback){
            event.callback(fd, *this, event.arg);
            continue;
        }else if(event.FLAG==ACCEPT && event.callback){
            event.callback(fd, *this, event.arg);
            continue;
        }else{
            continue;
        }
    }
    return 0;
}

#endif //EPOLL_TEST_H