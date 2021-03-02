#include <sys/time.h>
#include <iostream>
#include <stdio.h>
#include <signal.h>
#include <sys/time.h>
#include <iostream>
#include "Thread.h"
#include <queue>
#include <unordered_map>
#include <vector>


#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
            "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

#define MICRO_SECOND 1000000
#define MAX_THREAD_NUM 100

static const char* MEM_FAILURE = "system error: failed memory allocation\n";
static const char* SYS_CALL_FAILURE = "system error: failed system call\n";
static const char* ID_FAILURE = "thread library error: Can not find thread with the given ID\n";
static const char* MAIN_THREAD_FAILURE = "thread library error: Can not complete this operation on main thread.\n";
static const char* MAX_THREAD_FAILURE = "thread library error: Maximum amount of threads exceeded.\n";
static const char* TIME_FAILURE = "thread library error: quantum time should be positive.\n";

static Thread *runningThread;
static std::deque<Thread*> readyQueue;
static std::unordered_map<int, Thread*> blockedMap;
static std::vector<unsigned int> delId;
static struct itimerval timer;
static struct sigaction sigTime;
static sigset_t set;
static sigset_t pendingSet;
static unsigned int activeThreads;
static int totalQuantum;
static int* timeSig;

int errorHandler(const char*, bool, bool);
void switchThreads(bool, State);
int getNextId();
int initTimer(int);
void timerHandler(int);
void terminateProcess();
Thread* findThread(int, bool);
void releaseSyncBlock(Thread*);

/*
 * Description: This function initializes the thread library.
 * You may assume that this function is called before any other thread library
 * function, and that it is called exactly once. The input to the function is
 * the length of a quantum in micro-seconds. It is an errorHandler to call this
 * function with non-positive quantum_usecs.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs)
{
    runningThread = new(std::nothrow) Thread(0);
    if(runningThread == nullptr)
    {
        errorHandler(MEM_FAILURE, true, true);
    }
    if(sigemptyset(&set) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(sigaddset(&set, SIGVTALRM))
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    runningThread->incQuantumCounter();
    runningThread->setState(running);
    totalQuantum++;
    activeThreads++;
    return initTimer(quantum_usecs);
}

/*
 * Description: This function creates a new thread, whose entry point is the
 * function f with the signature void f(void). The thread is added to the end
 * of the READY threads list. The uthread_spawn function should fail if it
 * would cause the number of concurrent threads to exceed the limit
 * (MAX_THREAD_NUM). Each thread should be allocated with a stack of size
 * STACK_SIZE bytes.
 * Return value: On success, return the ID of the created thread.
 * On failure, return -1.
*/
int uthread_spawn(void (*f)(void))
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(activeThreads == MAX_THREAD_NUM)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(MAX_THREAD_FAILURE, false, false);
    }
    Thread *thread = new(std::nothrow) Thread(getNextId());
    if(thread == nullptr)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    address_t sp, pc;
    sp = (address_t)thread->getStack() + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    sigsetjmp(thread->getEnv(), thread->getId());
    (thread->getEnv()->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread->getEnv()->__jmpbuf)[JB_PC] = translate_address(pc);
    if (sigemptyset(&thread->getEnv()->__saved_mask) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    readyQueue.push_back(thread);
    activeThreads++;
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return thread->getId();
}


/*
 * Description: This function terminates the thread with ID tid and deletes
 * it from all relevant control structures. All the resources allocated by
 * the library for this thread should be released. If no thread with ID tid
 * exists it is considered as an errorHandler. Terminating the main thread
 * (tid == 0) will result in the termination of the entire process using
 * exit(0) [after releasing the assigned library memory].
 * Return value: The function returns 0 if the thread was successfully
 * terminated and -1 otherwise. If a thread terminates itself or the main
 * thread is terminated, the function does not return.
*/
int uthread_terminate(int tid)
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(tid > MAX_THREAD_NUM || tid < 0)
    {
        if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    if(tid == 0)
    {
        terminateProcess();
        exit(0);
    }
    Thread *thread = findThread(tid, true);
    if (thread == nullptr)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    delId.push_back((unsigned int)tid);
    if(thread->getState() == running)
    {
        switchThreads(true, ready);
    }
    releaseSyncBlock(thread);
    delete thread;
    activeThreads--;
    if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return 0;
}


/*
 * Description: This function blocks the thread with ID tid. The thread may
 * be resumed later using uthread_resume. If no thread with ID tid exists it
 * is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision
 * should be made. Blocking a thread in BLOCKED state has no
 * effect and is not considered as an errorHandler.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_block(int tid)
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(tid == 0)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(MAIN_THREAD_FAILURE, false, false);
    }
    Thread *thread = findThread(tid, true);
    if (thread == nullptr)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    thread->setBlock(true);
    if(thread->getState() == running)
    {
        switchThreads(false, blocked);
    }
    thread->setState(blocked);
    blockedMap[thread->getId()] = thread;
    if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return 0;
}

/*
 * Description: This function resumes a blockedMap thread with ID tid and moves
 * it to the READY state. Resuming a thread in a RUNNING or READY state
 * has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered as an errorHandler.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid)
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(tid > MAX_THREAD_NUM || tid < 0)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    Thread *thread = findThread(tid, false);
    if (thread == nullptr)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    if(thread->getState() == ready || thread->getState() == running)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return 0;
    }
    thread->setBlock(false);
    if(thread->isSync())
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return 0;
    }
    blockedMap.erase(thread->getId());
    thread->setState(ready);
    readyQueue.push_back(thread);
    if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return 0;
}


/*
 * Description: This function blocks the RUNNING thread until thread with
 * ID tid will move to RUNNING state (i.e.right after the next time that
 * thread tid will stop runningThread, the calling thread will be resumed
 * automatically). If thread with ID tid will be terminated before RUNNING
 * again, the calling thread should move to READY state right after thread
 * tid is terminated (i.e. it won’t be blockedMap forever). It is considered
 * as an errorHandler if no thread with ID tid exists or if the main thread (tid==0)
 * calls this function. Immediately after the RUNNING thread transitions to
 * the BLOCKED state a scheduling decision should be made.
 * Return value: On success, return 0. On failure, return -1.
*/
int uthread_sync(int tid)
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(tid == 0)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(MAIN_THREAD_FAILURE, false, false);
    }
    if(tid > MAX_THREAD_NUM || tid < 0)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    Thread *thread = findThread(tid, false);
    if (thread == nullptr)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    thread->addToWaitingList(runningThread->getId());
    runningThread->setSync(true);
    switchThreads(false, blocked);
    if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return 0;
}


/*
 * Description: This function returns the thread ID of the calling thread.
 * Return value: The ID of the calling thread.
*/
int uthread_get_tid()
{
    return runningThread->getId();
};


/*
 * Description: This function returns the total number of quantums that were
 * started since the library was initialized, including the current quantum.
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number
 * should be increased by 1.
 * Return value: The total number of quantums.
*/
int uthread_get_total_quantums()
{
    return totalQuantum;
};


/*
 * Description: This function returns the number of quantums the thread with
 * ID tid was in RUNNING state. On the first time a thread runs, the function
 * should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state
 * when this function is called, include also the current quantum). If no
 * thread with ID tid exists it is considered as an errorHandler.
 * Return value: On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid)
{
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(tid < 0 || tid > MAX_THREAD_NUM)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    Thread *found = findThread(tid, false);
    if(found == nullptr)
    {
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        return errorHandler(ID_FAILURE, false, false);
    }
    if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return found->getQuantumCounter();
}

/*
 * SIGVTALRM signal handler.
*/
void timerHandler(int sig)
{
    switchThreads(false, ready);
}

/*
 * Init helper.
 */
int initTimer(int time)
{
    if(time <= 0)
    {
        return errorHandler(TIME_FAILURE, true, false);
    }
    if(sigemptyset(&sigTime.sa_mask) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    sigTime.sa_handler = &timerHandler;
    int micro_sec = time % MICRO_SECOND;
    int sec = (time - micro_sec)/MICRO_SECOND;
    timer.it_value.tv_sec = sec;		// first time interval, seconds part
    timer.it_value.tv_usec = micro_sec;		// first time interval, microseconds part
    timer.it_interval.tv_sec = sec;	// following time intervals, seconds part
    timer.it_interval.tv_usec = micro_sec;	// following time intervals, microseconds part
    if (sigaction(SIGVTALRM, &sigTime, nullptr) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if (setitimer (ITIMER_VIRTUAL, &timer, nullptr))
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    return 0;
}

/*
 * Switches between the running thread and the next thread on the ready list.
 * Sets the state of the currently running thread to State.
 * If terminate == true, the currently running thread will be terminated,
 * otherwise it will be put to ready list or blocked map according to State.
 */
void switchThreads(bool terminate, State state = ready)
{
    if(sigemptyset(&pendingSet) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    if(sigprocmask(SIG_BLOCK, &set, NULL) < 0)
    {
        errorHandler(SYS_CALL_FAILURE, true, true);
    }
    totalQuantum++;
    if(terminate)
    {
        delete runningThread;
        runningThread = readyQueue.front();
        readyQueue.pop_front();
        runningThread->setState(running);
        if(setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        runningThread->incQuantumCounter();
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        siglongjmp(runningThread->getEnv(), 1);
    }
    int ret_val = sigsetjmp(runningThread->getEnv(), 1);
    if (ret_val == 0)
    {
        runningThread->setState(state);
        if (state == ready)
        {
            readyQueue.push_back(runningThread);
        }
        else
        {
            blockedMap[runningThread->getId()] = runningThread;
            if(setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0)
            {
                errorHandler(SYS_CALL_FAILURE, true, true);
            }
        }
        runningThread = readyQueue.front();
        readyQueue.pop_front();
        runningThread->setState(running);
        runningThread->incQuantumCounter();
        releaseSyncBlock(runningThread);
        if(sigpending(&pendingSet) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        int ret = sigismember(&pendingSet, SIGVTALRM);
        if(ret == 1)
        {
            sigwait(&set, timeSig);
        }
        if(ret < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        if(sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
        {
            errorHandler(SYS_CALL_FAILURE, true, true);
        }
        siglongjmp(runningThread->getEnv(), 1);
    }
}

/*
 * Handles errors.
 * Prints errMsg to stderr.
 * If terminate == true, the process will terminate.
 * If ifExit==true, the program will exit, otherwise -1 will be returned.
 */
int errorHandler(const char* errMsg, bool terminate, bool ifExit)
{
    fprintf(stderr, errMsg);
    if(terminate)
    {
        terminateProcess();
    }
    if(ifExit)
    {
        exit(1);
    }
    return -1;
}

/*
 * Returns next available ID for a new thread.
 */
int getNextId()
{
    if(delId.empty())
    {
        return activeThreads;
    }
    unsigned int min = MAX_THREAD_NUM;
    for(unsigned int i = 0; i < delId.size(); i++)
        if (delId[i] < min)
        {
            min = delId[i];
        }
    for(unsigned int i = 0; i < delId.size(); i++)
        if (delId[i] == min)
        {
            delId.erase(delId.begin() + i);
            break;
        }
    return min;
}

/*
 * Finds a thread with the ID tid.
 * If remove==true, the thread will be removed from its data structure.
 */
Thread* findThread(int tid, bool remove)
{
    if(runningThread->getId() == tid)
    {
        return runningThread;
    }
    // search in blocked
    std::unordered_map<int, Thread*>::const_iterator found = blockedMap.find(tid);
    if (found != blockedMap.end())
    {
        Thread* ret = found->second;
        if(remove)
        {
            blockedMap.erase(found);
        }
        return ret;
    }
    // search in ready
    for(auto it = readyQueue.cbegin(); it != readyQueue.cend(); ++it)
    {
        if((*it)->getId() == tid)
        {
            Thread* ret = *it;
            if(remove)
            {
                readyQueue.erase(it);
            }
            return ret;
        }
    }
    return nullptr;
}

/*
 * Terminates the library and releases all allocated memory.
 */
void terminateProcess()
{
    for (auto it : blockedMap)
    {
        delete it.second;
    }
    blockedMap.clear();
    while(!readyQueue.empty())
    {
        readyQueue.pop_front();
    }
    delete runningThread;
}

/*
 * Releases all synced threads, synced by thread.
 */
void releaseSyncBlock(Thread* thread)
{
    int temp = thread->popWaitingId();
    while(temp != -1)
    {
        std::unordered_map<int, Thread*>::const_iterator found = blockedMap.find(temp);
        if (found != blockedMap.end())
        {
            Thread *ret = found->second;
            ret->setSync(false);
            if(!ret->isBlock())
            {
                blockedMap.erase(found);
                ret->setState(ready);
                readyQueue.push_back(ret);
            }
        }
        temp = runningThread->popWaitingId();
    }
}
