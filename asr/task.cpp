#include "task.h"

void ForkJoinTask::fork(ForkJoinTask *task)
{
    _executor->_mgr->invoke(task);
}

void ForkJoinTask::join()
{
check_done:
    asm ("xorl %eax, %eax");
    asm ("lock cmpxchg %eax, _finished");
    asm ("jnz not_done");
    asm ("ret");
not_done:
    //_executor->exec_next();
    asm ("jmp check_done");
}