/*
*	qpsnr (C) 2010 E. Oriani, ema <AT> fastwebnet <DOT> it
*
*	This file is part of qpsnr.
*
*	qpsnr is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	qpsnr is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with qpsnr.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _MT_H_
#define _MT_H_

#include <pthread.h>
#include <semaphore.h>
#include <list>
#include <vector>
#include <errno.h>
#include <exception>
#include <string>

namespace mt {

	class mt_exception : public std::exception {
		const std::string	_what;
	public:
		mt_exception(const std::string& what) : _what(what) {
		}

  		virtual const char* what() const throw() {
    			return _what.c_str();
  		}

		~mt_exception() throw () {
		}
	};

	class Semaphore {
		sem_t	_sem;

		Semaphore(const Semaphore&);
		Semaphore& operator=(const Semaphore&);
	public:
		Semaphore() {
			if (0 != sem_init(&_sem, 0, 1))
				throw mt_exception("Semaphore: Semaphore()");
		}

		void push(void) {
			if (0 != sem_wait(&_sem))
				throw mt_exception("Sempahore: push");
		}

		bool trypush(void) {
			if (0 == sem_trywait(&_sem)) {
				return true;
			} else if (errno == EAGAIN) {
				return false;
			}
			throw mt_exception("Semaphore: trywait");
		}

		void pop(void) {
			if (0 != sem_post(&_sem))
				throw mt_exception("Sempahore: pop");
		}
		
		~Semaphore() {
			sem_destroy(&_sem);
		}
	};

	class Mutex {
		pthread_mutex_t	_mtx;

		Mutex(const Mutex&);
		Mutex& operator=(const Mutex&);
	public:
		Mutex() {
			if (0 != pthread_mutex_init(&_mtx, NULL))
				throw mt_exception("Mutex: Mutex()");
		}

		void lock(void) {
			if (0 != pthread_mutex_lock(&_mtx))
				throw mt_exception("Mutex: lock");
		}

		void unlock(void) {
			if (0 != pthread_mutex_unlock(&_mtx))
				throw mt_exception("Mutex: unlock");
		}

		~Mutex() {
			pthread_mutex_destroy(&_mtx);
		}
	};

	class ScopedLock {
		Mutex& _mtx;
		
		ScopedLock(const ScopedLock&);
		ScopedLock& operator=(const ScopedLock&);
	public:
		ScopedLock(Mutex& mtx) : _mtx(mtx) {
			_mtx.lock();
		}

		~ScopedLock() {
			_mtx.unlock();
		}
	};

	class Thread {
		pthread_t	_th;

		static void* exec(void* par) throw() {
			try {
				Thread *p = (Thread*)par;
				p->run();
			} catch (...) {
			}
			return 0;
		}

		Thread(const Thread&);
		Thread& operator=(const Thread&);
	public:
		Thread() : _th(0) {
		}

		virtual void run(void) = 0;

		void start(void) {
			if (0 != pthread_create(&_th, NULL, &exec, this))
					throw mt_exception("Thread: start");
		}

		void join(void) {
			if (0!=_th) pthread_join(_th, NULL);
		}
		
		virtual ~Thread() {
		}
	};

	class ThreadPool {
	public:
		class Job {
			friend	class	ThreadPool;
			Semaphore	_sem;
			volatile bool	_on_hold;
			const bool	_to_be_deleted;
		public:
			Job(const bool& to_be_deleted = false) : _on_hold(true), _to_be_deleted(to_be_deleted) {
				_sem.push();
			}

			virtual void run(void) = 0;

			/* Just rememeber that
			- void wait(void)
			- bool is_running(void)
			   can only be used when the ownership of the
			   ThreadPool::Job instance is external (eg.
			   to_be_deleted = false).
			   Otherwise this will lead to a crash!
			*/

			// We pop the semaphore again in case someone else
			// will ask to wait again...
			void wait(void) {
				_sem.push();
				_sem.pop();
			};

			bool is_running(void) {
				if (_on_hold) return false;
				if (false == _sem.trypush())
					return true;
				_sem.pop();
				return false;
			}

			virtual ~Job() {
			}
		};
	private:
		Mutex		_list_mtx;
		Semaphore	_list_sem;
		std::list<Job*>	_list_jobs;

		// the following variable is not mutex
		// protected because is sort of write-only
		// by main ThreadPool thread, see destructor
		volatile bool		_tp_quit;
		const unsigned int	_n_execs;
		std::vector<pthread_t>	_th_ids;

		bool get_job(Job** _job) {
			ScopedLock _sl(_list_mtx);
			if (!_list_jobs.empty()) {
				*_job = _list_jobs.front();
				_list_jobs.pop_front();
				return true;
			}
			return false;
		}

		// Technically we should declare it as extern "C" but we
		// don't care as seen as the function pointer will correspond
		// to a proper C-like function even if the name will be C++ like
		static void* job_exec(void* par) throw() {
			try {
				ThreadPool *p = (ThreadPool*)par;
				while(true) {
					// first push on the semaphore
					p->_list_sem.push();
					// check if we have to quit
					if (p->_tp_quit) return 0;
					// get an element to process
					Job	*curJob = 0;
					if (p->get_job(&curJob)) {
						// we need to save the delete_job varibale because
						// after the semaphore has been popped the object 
						// could not exist anymore
						const bool delete_job = curJob->_to_be_deleted;
						try {
							curJob->_on_hold = false;
							curJob->run();
						} catch(...) {
						}
						// if the following instruction throws is better 
						// to let the user know because this means that
						// the semaphore is not valid anymore...this should
						// never happen...
						curJob->_sem.pop();
						// if it has to be deleted do it!
						if (delete_job) delete curJob;
					}
					// check if we have to quit
					if (p->_tp_quit) return 0;
				}
			} catch(...) {
			}
			return 0;
		}

		ThreadPool(const ThreadPool&);
		ThreadPool& operator=(const ThreadPool&);
	public:
		// Just take into account that semaphores are not syscall immune
		// so when you run in debug mode you can have exceptions thrown on
		// push because of system interrruption! Don't get scared!
		ThreadPool(const unsigned int& n_execs) : _tp_quit(false), _n_execs(n_execs), _th_ids(n_execs) {
			if (_n_execs == 0 || _n_execs > 256)
				throw mt_exception("ThreadPool: invalid number of n_execs");
			// create the job_exec threads
			for (unsigned int i = 0; i < _n_execs; ++i)
				if (0 != pthread_create(&_th_ids[i], NULL, &job_exec, this)) {
					for (unsigned int j=0; j < i; ++j)
						pthread_cancel(_th_ids[j]);
					throw mt_exception("ThreadPool: could not start all specified n_execs");
				}
		}

		void add(Job* job) {
			ScopedLock _sl(_list_mtx);
			_list_jobs.push_back(job);
			_list_sem.pop();
		}

		~ThreadPool() {
			_tp_quit = true;
			for (unsigned int i = 0; i < _n_execs; ++i)
				_list_sem.pop();
			for (unsigned int i = 0; i < _n_execs; ++i)
				pthread_join(_th_ids[i], NULL);
			// potentialy unsafe...this could throw...but should never...
			ScopedLock _sl(_list_mtx);
			for (std::list<Job*>::iterator it = _list_jobs.begin(); it != _list_jobs.end(); ++it)
				if ((*it)->_to_be_deleted) delete *it;
		}
	};
}

#endif //_MT_H_
