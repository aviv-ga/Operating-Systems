#include "MapReduceFramework.h"
#include "Barrier.h"
#include <atomic>
#include <algorithm>
#include <semaphore.h>
#include <iostream>


#define SYS_ERR_MSG "Failed System Call."
#define MEM_ERR_MSG "Failed Memory Allocation."

struct ThreadContext
{
    std::atomic<unsigned int>* atomic_counter;
    const MapReduceClient* client;
    const InputVec *input;
    OutputVec* output;
    IntermediateVec* intermediateVec;
    IntermediateVec* intermediateVecArr;
    std::vector<IntermediateVec*>* queue;
    Barrier* barrier;
    sem_t *semaphore;
    pthread_mutex_t *q_mutex;
    pthread_mutex_t *e_mutex;
    int mtlvl;
    bool shuffle;
    bool* end_shuffle;
};

bool sortIntermediatePairs(IntermediatePair& p1, IntermediatePair& p2);
bool checkIntermediateVecs(ThreadContext* context);
K2* find_max_k2_val(ThreadContext*);
IntermediateVec* create_vec_4_reduce(ThreadContext*, K2*);
void map_phase(ThreadContext*);
void reduce_phase(ThreadContext*);
void shuffle(ThreadContext*);
void *work(void*);



/**
 * Run map, reduce framework.
 * @param client object that implements MapReduceClient.h.
 * @param inputVec data structure of type InputVec which holds all relevant keys and values.
 * @param outputVec data structure of type OutputVec which holds all relevant keys and values.
 * @param multiThreadLevel numbers of threads to create.
 */
void runMapReduceFramework(const MapReduceClient& client,
                           const InputVec& inputVec, OutputVec& outputVec,
                           int multiThreadLevel)
{
    pthread_t threadArr[multiThreadLevel - 1];
    ThreadContext contextArr[multiThreadLevel];
    IntermediateVec intermediateArr[multiThreadLevel];
    std::vector<IntermediateVec*> queue;
    Barrier barrier(multiThreadLevel);
    std::atomic<unsigned int> atomic_counter(0);
    sem_t semaphore;
    if(sem_init(&semaphore, 0, 0) < 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
    pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t emit_mutex = PTHREAD_MUTEX_INITIALIZER;
    bool e_shuff = false;

    for (int i = 0; i < multiThreadLevel; ++i)
    {
        intermediateArr[i] = IntermediateVec();
        if (i == 0) {
            contextArr[i] = {&atomic_counter, &client, &inputVec, &outputVec, &intermediateArr[i],
                             intermediateArr, &queue, &barrier, &semaphore, &queue_mutex,
                             &emit_mutex, multiThreadLevel, true, &e_shuff};
        } else {
            contextArr[i] = {&atomic_counter, &client, &inputVec, &outputVec, &intermediateArr[i],
                             intermediateArr, &queue, &barrier, &semaphore, &queue_mutex,
                             &emit_mutex, multiThreadLevel, false, &e_shuff};
        }
    }
    for (int i = 0; i < multiThreadLevel - 1; ++i)
    {
        int ret_val = pthread_create(threadArr + i, NULL, work, contextArr + (i + 1));
        if (ret_val)
        {
            fprintf(stderr,SYS_ERR_MSG);
            sem_destroy(&semaphore);
            pthread_mutex_destroy(&emit_mutex);
            pthread_mutex_destroy(&queue_mutex);
        }
    }
    work((void *) &contextArr[0]); //main thread goes to work!
    for (int i = 0; i < multiThreadLevel - 1; ++i)
    {
        pthread_join(threadArr[i], NULL);
    }

    if(sem_destroy(&semaphore) != 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
    if(pthread_mutex_destroy(&emit_mutex) != 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
    if(pthread_mutex_destroy(&queue_mutex) != 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
}


/**
 * Affix Intermediate pair created by Map() in the MapReduceFramework
 * @param key of K2*.
 * @param value of V2*.
 * @param context Threadcontext.
 */
void emit2 (K2* key, V2* value, void* context)
{
    auto c = (IntermediateVec*)context;
    c->push_back(IntermediatePair(key, value));
}

/**
 * Affix Output pair created by Map() in the MapReduceFramework
 * @param key of K3*.
 * @param value of V3*.
 * @param context Threadcontext.
 */
void emit3 (K3* key, V3* value, void* context)
{
    if(pthread_mutex_lock(((ThreadContext*)context)->e_mutex) != 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
    auto o = ((ThreadContext*)context)->output;
    o->push_back(OutputPair(key, value));
    if(pthread_mutex_unlock(((ThreadContext*)context)->e_mutex) != 0)
    {
        fprintf(stderr,SYS_ERR_MSG);
        exit(1);
    }
}

/**
 * The function associated with all threads.
 * @param arg Threadcontext.
 */
void *work(void* arg)
{
    auto tc = (ThreadContext*) arg;
    map_phase(tc);
    std::sort(tc->intermediateVec->begin(), tc->intermediateVec->end(), sortIntermediatePairs);
    tc->barrier->barrier();
    if(tc->shuffle)
    {
        shuffle(tc);
    }
    reduce_phase(tc);
    return nullptr;
}

/**
 * Find all matching <K2,V2> pairs in all intermediate vectors with maximal K2 value.
 * @param context Threadcontext.
 */
void shuffle(ThreadContext* context)
{
    bool empty = checkIntermediateVecs(context);
    K2* max_key_val;
    while (!empty)
    {
        max_key_val = find_max_k2_val(context);
        IntermediateVec* readyForReduce = create_vec_4_reduce(context, max_key_val);
        if(pthread_mutex_lock(context->q_mutex) != 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
        context->queue->push_back(readyForReduce);
        if(pthread_mutex_unlock(context->q_mutex) != 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
        if(sem_post(context->semaphore) < 0)
        {
            printf(SYS_ERR_MSG);
            exit(1);
        }
        empty = checkIntermediateVecs(context);
    }
    (*context->end_shuffle) = true;
    for(int i = 0; i < context->mtlvl; ++i)
    {
        if(sem_post(context->semaphore) < 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
    }
}

/**
 * Map input vector to all threads.
 * @param context Threadcontext.
 */
void map_phase(ThreadContext* context)
{
    unsigned int old_value = (*(context->atomic_counter));
    while(old_value < context->input->size())
    {
        (*(context->atomic_counter))++;
        const InputVec* inputV = context->input;
        const InputPair pair = inputV->at(old_value);
        (*(context->client)).map(pair.first, pair.second, context->intermediateVec);
        old_value = (*(context->atomic_counter));
    }
}
/**
 * Reduce a ready Intermediate vector (while queue is not empty) to an output pair.
 * @param context Threadcontext.
 */
void reduce_phase(ThreadContext* context)
{
    while(!context->queue->empty() || !*(context->end_shuffle))
    {
        IntermediateVec* reduce_me = nullptr;
        if(sem_wait(context->semaphore) != 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
        if(pthread_mutex_lock(context->q_mutex) != 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
        if(!context->queue->empty())
        {
            reduce_me = context->queue->back();
            context->queue->pop_back();
        }
        if(pthread_mutex_unlock(context->q_mutex) != 0)
        {
            fprintf(stderr,SYS_ERR_MSG);
            exit(1);
        }
        if(reduce_me != nullptr)
        {
            context->client->reduce(reduce_me, context);
            delete reduce_me;
        }
    }
}

/**
 * Find the maximum K2 value in Intermediate vectors of all threads.
 * @param context Threadcontext
 * @return pointer to the maximum K2 found.
 */
K2* find_max_k2_val(ThreadContext* context)
{
    K2* max = nullptr;
    bool flag = true;
    for(int i = 0; i < context->mtlvl; ++i)
    {
        if(!context->intermediateVecArr[i].empty())
        {
            K2* val = context->intermediateVecArr[i].back().first;
            if (flag)
            {
                max = val;
                flag = false;
            }
            else
            {
                if(*max < *val)
                {
                    max = val;
                }
            }
        }
    }
    return max;
}

/**
 * Creates an Intermediate vector of matching key values to be prepared for reduce.
 * @param context Threadcontext
 * @param max_val the key to find.
 * @return pointer to Intermediate vector which is ready for reduce.
 */
IntermediateVec* create_vec_4_reduce(ThreadContext* context, K2* max_val)
{
    auto *readyForReduce = new(std::nothrow) IntermediateVec();
    if(readyForReduce == nullptr)
    {
        fprintf(stderr,MEM_ERR_MSG);
        exit(1);
    }
    for (int i = 0; i < context->mtlvl; ++i)
    {
        IntermediateVec *vec = &(context->intermediateVecArr[i]);
        if (!vec->empty())
        {
            IntermediatePair* push_pair = &(vec->back());
            while(!vec->empty() && !(*(push_pair->first) < *(max_val)))
            {
                readyForReduce->push_back(*push_pair);
                vec->pop_back();
                if(vec->empty())
                {
                    break;
                }
                push_pair = &(vec->back());
            }
        }
    }
    return readyForReduce;
}

/**
 * Comparator for Intermediate pairs.
 * @param p1 IntermediatePair
 * @param p2 IntermediatePair
 * @return true if p1.first < p2.first, false otherwise.
 */
bool sortIntermediatePairs(IntermediatePair& p1, IntermediatePair& p2)
{
    K2* firstElem = (p1.first);
    K2* secondElem = (p2.first);
    return *firstElem < *secondElem;
}

/**
 * Check if exists non empty Intermediate vector.
 * @param context Threadcontext.
 * @return true all vectors are empty, false otherwise.
 */
bool checkIntermediateVecs(ThreadContext* context)
{
    for(int i = 0; i < context->mtlvl; ++i)
    {
        if (!context->intermediateVecArr[i].empty())
        {
            return false;
        }
    }
    return true;
}
