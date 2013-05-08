#ifndef _ASR_TASK_H_
#define _ASR_TASK_H_

#include <vector>

class ForkJoinTaskExecutor;
class ForkJoinTaskManager;

class ForkJoinTask
{
public:
    ForkJoinTask() : _finished(false) {}
    virtual ~ForkJoinTask() {}
    
    virtual void exec() = 0;
    virtual void fork(ForkJoinTask *task);
    virtual void join();

    ForkJoinTaskExecutor *_executor;
    bool _finished;
};

class ForkJoinTaskExecutor
{
public:
    ForkJoinTaskExecutor(ForkJoinTaskManager *mgr) : _mgr(mgr) {}
    
    void run()
    {
        
    }
    
    ForkJoinTaskManager *_mgr;
};

class ForkJoinTaskManager
{
public:
    ForkJoinTaskManager(int poolSize);
    
    void invoke(ForkJoinTask *task);
private:
    std::vector<ForkJoinTaskExecutor*> _pool;
};

#endif // !defined(_ASR_TASK_H_)
