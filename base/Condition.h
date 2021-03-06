#ifndef MOXIE_CONDITION_H
#define MOXIE_CONDITION_H
#include <pthread.h>

namespace moxie {

class Mutex;

class Condition {
public:
    Condition(Mutex& mutex);
    ~Condition() ;

    void wait();
    // returns true if time out, false otherwise.
    bool waitForSeconds(int seconds);

    void notify() ;
    void notifyAll();

private:
    Mutex& mutex_;
    pthread_cond_t cond_;
};

}
#endif  // MOXIE_CONDITION_H
