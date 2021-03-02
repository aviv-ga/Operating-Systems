//
// Created by cveksler on 4/23/18.
//

#include "Thread.h"

int Thread::popWaitingId()
{
    if(waitingList.empty())
    {
        return -1;
    }
    int temp = waitingList.front();
    waitingList.pop_front();
    return temp;
}

void Thread::addToWaitingList(int tid)
{
    waitingList.push_back(tid);
}




