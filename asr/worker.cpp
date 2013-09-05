// ASR - Digital Signal Processor
// Copyright (C) 2002-2013	Paul Ciarlo <paul.ciarlo@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.	 If not, see <http://www.gnu.org/licenses/>.

#include "worker.h"

Lock_T Worker::_job_lock;
Condition_T Worker::_cr_job_rdy;
Condition_T Worker::_job_rdy;
Condition_T Worker::_job_done;
std::queue<Worker::job*> Worker::_critical_jobs;
std::queue<Worker::job*> Worker::_idle_jobs;
pthread_once_t Worker::once_control = PTHREAD_ONCE_INIT; 
bool Worker::_running = true;
std::vector<Worker*> Worker::_workers;
std::vector<Worker*> Worker::_cr_workers;
volatile int Worker::_suspend_count = 0;
