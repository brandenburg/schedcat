#ifndef EVENT_H
#define EVENT_H

#include <queue>

template <class time_t>
class Event
{
  public:
    virtual void fire(const time_t &cur_time) {};  /* callback */
};


template <class time_t>
class Timeout
{
  private:
    time_t          fire_time;
    Event<time_t>   *handler;

  public:
    Timeout(time_t when, Event<time_t> *what)
        : fire_time(when), handler(what) {}

    const time_t& time() const
    {
        return fire_time;
    }

    Event<time_t>& event() const
    {
        return *handler;
    }

    bool operator<(const Timeout<time_t> &that) const
    {
        return this->time() < that.time();
    }

    bool operator>(const Timeout<time_t> &that) const
    {
        return this->time() > that.time();
    }
};


#endif
