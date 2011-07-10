#include "worker.h"

pthread_mutex_t Worker::_job_lock;
pthread_cond_t Worker::_cr_job_rdy;
pthread_cond_t Worker::_job_rdy;
pthread_cond_t Worker::_job_done;
std::queue<Worker::job*> Worker::_critical_jobs;
std::queue<Worker::job*> Worker::_idle_jobs;
pthread_once_t Worker::once_control = PTHREAD_ONCE_INIT; 
