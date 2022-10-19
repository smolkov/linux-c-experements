/** 
 * Demo Code
 * typical code
 */

#include <stdlib.h> 
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <syslog.h>
#include <inttypes.h>
#include <string.h>

static int startWorker();

/**
 * Static objects
 */
static uint64_t shared_status_both = 0;
static pthread_t worker_thread;
static volatile int thread_enabled = 0;
static pthread_mutex_t mutex; 

/*
 * Global varibale
 */
int32_t global_var = 0;


/**
 * doWorker Thread
 *
 */
void * doWorkerThread(void *arg) {

	(void) arg;

	int counter = 0;

	printf("Thread startes...\n");

	// oft findet man Warte Code 
	while (! thread_enabled) {
		sleep(1);
	}
	
	printf("Thread enabled...\n");


	while(1) {
		int err;

		// Thread work

		err = pthread_mutex_lock(&mutex);
		assert(err == 0); // Warning -> Lock

		shared_status_both++;
		
		// short lock <-> Achtung: Priority inversion
		 sleep(1);
		// siehe : pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);

		err = pthread_mutex_unlock(&mutex);
		assert(err == 0);

		// verschalte Locks -> Gefährlich -> Lock order! usw...


		// Note: Prio Änderunge für Locks-> Generell ansehen und versuchen zu entfernen, wenn man ein Modul ändert
                //  -> Kd einbeziehen

		usleep(5000);
		
		if (++counter > 150) {
			// bad things here...
			// abort();
			// global_var = 0;
			// memset(&global_var, 0, 10);
			counter = 0;
		}

		
	}
}


/**
 * doLoop
 *
 */
void doLoop() {

	// Thread darf starten...
	thread_enabled = 1;


	while(1) {
		int err;

		// Thread communication

		err = pthread_mutex_lock(&mutex);
		assert(err == 0);   // warning: EDEADLK - The current thread already owns the mutex.
          			    // -> "man kann es übertreiben"
						// man pthread_mutex_lock

		if (shared_status_both) {
			// Auch hier auf  Priority inversion achten
		  	printf("Update from Worker (%" PRIu64") global_var: %" PRId32 ")\n", shared_status_both, global_var);
			shared_status_both = 0;
		}
		
		
		err = pthread_mutex_unlock(&mutex);
		assert(err == 0);

		global_var++;
		
		usleep(50000);
	
	}

}


int main() {


	if (startWorker() < 0 /* IDA_OK*/) {
		// Wenn es nie passieren darf und ein Fehler ist, dann Programm-Abbruch
		abort();             // Fatal -> mit Corefile für Analysen  -> exit 128+6
		exit(EXIT_FAILURE);  // Behandlung vom Aufrufer z.B. .scec.sh usw
	}

	/* example control loop */
	doLoop();


	return EXIT_SUCCESS;
}


static int startWorker() {


	int err;
	pthread_mutexattr_t attr_mutex;
	
	/** immer Man Page lesen!!! */
	err = pthread_create(&worker_thread, NULL, &doWorkerThread, NULL); 
	
	assert(err == 0);	// Fatal -> wird sofort abgebrochen! -wie abort()

	if (err != 0) {	
		// interrnal error log  -> Soll uns Entwickler helfen! -
                // Achten auf LOG-Fluten !
		syslog(LOG_ERR, "In %s pthread_create failed with %d",__FUNCTION__,errno);
		return err;
	}  


	// create mutex
	err = pthread_mutexattr_init(&attr_mutex);
	assert( err == 0 );   // e.g. man page -> no EINTR
	// -> exit_fnc_with_errorno();

	// Note
	// Nie !!!!! -> NDEBUG macro ->  the macro assert() generates no code
	assert( pthread_mutexattr_init(&attr_mutex) == 0);


	err = pthread_mutexattr_settype(&attr_mutex, PTHREAD_MUTEX_RECURSIVE);
	assert( err == 0 );   
	// -> exit_fnc_with_errorno();


//	Use with care ->  not portable
//	err = pthread_mutexattr_setprotocol(&attr_mutex, PTHREAD_PRIO_INHERIT);
//	assert( err == 0 );   
// --> man pthread_mutexattr_getprotocol

	err = pthread_mutex_init(&mutex, &attr_mutex);
	assert( err == 0 );   
	// -> exit_fnc_with_errorno();

	return err;
}

