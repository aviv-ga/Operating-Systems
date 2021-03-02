//
// Created by cveksler on 4/23/18.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#include <setjmp.h>
#include <malloc.h>
#include <vector>
#include <deque>

#define STACK_SIZE 4096

typedef unsigned long address_t;

enum State {running, blocked, ready};
class Thread
{
public:
    /**
     * Constructor of Thread object with a given ID.
     * @param tid ID of the thread.
     */
    explicit Thread(int tid) : id(tid), quantumCounter(0), state(ready), stack(), env(), waitingList() {}
    /**
     * Destructor of Thread object.
     */
    ~Thread() = default;
    /**
     * @return Environment of the Thread.
     */
    __jmp_buf_tag *getEnv() { return env; }
    /**
     * @return The stack of the Thread.
     */
    const char *getStack() const { return stack; }
    /**
     * @return The ID of the Thread.
     */
    int getId() const { return id; }
    /**
     * @return Quantum counter of the Thread.
     */
    int getQuantumCounter() const { return quantumCounter; }
    /**
     * @return The state of the Thread.
     */
    State getState() const { return state; }
    /**
     * Add an integer to the waitingList.
     * @param tid an Id to add to the waiting list.
     */
    void addToWaitingList(int tid);
    /**
     * Pops the next Id in the waiting list vector.
     * @return Id in waiting list.
     */
    int popWaitingId();
    /**
     * Set new state to the Thread.
     * @param state the state to set.
     */
    void setState(State state) { Thread::state = state; }
    /**
     * Increment Thread's quantumCounter.
     */
    void incQuantumCounter() { quantumCounter++; }

    bool isSync() const { return sync; }

    void setSync(bool sync) { Thread::sync = sync; }

    bool isBlock() const { return block; }

    void setBlock(bool block) { Thread::block = block; }


private:
    /** Thread's ID. */
    int id;
    /** Thread's quantum. */
    int quantumCounter;
    /** Thread's current state. */
    State state;
    /** Thread's stack buffer */
    char stack[STACK_SIZE];
    bool sync;
    bool block;
    /** Thread's environment. */
    sigjmp_buf env;
    /**
     * Data structure that holds blocked Id's.
     */
    std::deque<int> waitingList;

};


#endif //EX2_THREAD_H
