#include "worker.h"

Lock_T Worker::_job_lock;
Condition_T Worker::_cr_job_rdy;
Condition_T Worker::_job_rdy;
Condition_T Worker::_job_done;
std::queue<Worker::job*> Worker::_critical_jobs;
std::queue<Worker::job*> Worker::_idle_jobs;
pthread_once_t Worker::once_control = PTHREAD_ONCE_INIT; 
