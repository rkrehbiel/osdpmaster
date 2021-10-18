#ifndef SYNC_QUEUE_H
#define SYNC_QUEUE_H

#include <pthread.h>
#include <queue>

template <class MEMB> class sync_queue: public std::queue < MEMB > {
protected:
	typedef std::queue < MEMB > super;
	pthread_mutex_t m_lock;
	pthread_cond_t m_signal;
public:
	sync_queue<MEMB>() {
		pthread_mutex_init(&m_lock, NULL);
		// cond init is a little spicier, I need to specify
		// that timedwait operations are based on
		// CLOCK_MONOTONIC
		pthread_condattr_t attrs;
		pthread_condattr_init(&attrs);
		pthread_condattr_setclock(&attrs, CLOCK_MONOTONIC);
		pthread_cond_init(&m_signal, &attrs);
		pthread_condattr_destroy(&attrs);
	}
	bool empty(void) {
		pthread_mutex_lock(&m_lock);
		bool s = super::empty();
		pthread_mutex_unlock(&m_lock);
		return s;
	}
	size_t size(void) {
		pthread_mutex_lock(&m_lock);
		size_t s = super::size();
		pthread_mutex_unlock(&m_lock);
		return s;
	}
	MEMB &front(void) {
		// This queue only implements single-reader-multiple-writer
		// (that's all the front()/pop() paradigm supports)
		// so locking isn't required for front(), only this one thread
		// will be reading.
		return super::front();
	}
	void push(const MEMB &v) {
		pthread_mutex_lock(&m_lock);
		super::push(v);
		pthread_cond_signal(&m_signal);
		pthread_mutex_unlock(&m_lock);
	}
	void pop(void) {
		pthread_mutex_lock(&m_lock);
		super::pop();
		pthread_mutex_unlock(&m_lock);
	}
	bool wait(uint32_t ms) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		ldiv_t qr = ldiv(ms, 1000);
		ts.tv_sec += qr.quot;
		ts.tv_nsec += qr.rem * 1000;
		if(ts.tv_nsec >= 1000000000) {
			ts.tv_nsec -= 1000000000;
			ts.tv_sec++;
		}
		bool ok;
		pthread_mutex_lock(&m_lock);
		for(;;) {
			int i = pthread_cond_timedwait(&m_signal, &m_lock, &ts);
			ok = !super::empty();
			if(i == 0)
				break;
			if(i == ETIMEDOUT)
				break;
			// Some other error, like EINTR
		}
		pthread_mutex_unlock(&m_lock);
		return ok;
	}

};

#endif // SYNC_QUEUE_H
