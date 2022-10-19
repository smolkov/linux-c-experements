/** 
 * Demo Code
 * Thread msg send to main loop
 * Main loop send msg to thread
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
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>

static int startWorker(void);

/**
 * Thread data
 */
typedef struct {
	int unix_fd;	
	//...
} threadData_t;


/**
 * Static objects
 */
static pthread_t worker_thread;
static int socket_vector[2];

/*
 * Global varibale
 */
int32_t global_var = 0;


/**
 * doWorker Thread
 *
 */
void * doWorkerThread(void *arg) {

	// Übergaben der Daten an den Thread 
        // keine statischen / gloable Daten 
	threadData_t  *threadData = arg;
           

	int32_t counter = 0;
	struct pollfd fds[1];
	int timeout = -1; 	/* infinite timeout */

	// work loop from thread
	while(1) {

		int err;

		fds[0].fd = threadData->unix_fd;
		fds[0].events = POLLIN;

		// TEMP_FAILURE_RETRY -> EINTR (! -> nie vergessen)
		err = TEMP_FAILURE_RETRY(poll(fds, 1, timeout));

		if (err == -1) {
			/* should never fail -> man page */		
			syslog(LOG_ERR, "In %s poll failed with %d", __FUNCTION__, errno);	
			abort(); 					
		} 

		// timeout
		if (err == 0) {
				
			err = TEMP_FAILURE_RETRY(write(threadData->unix_fd, &counter, sizeof(int32_t)));
			assert(err); // Blocking write on overflow
			if (err < 0 ) {
				// -z.B. man 2 write
				abort();
			}


			// 	Timeout for tests
			if (++counter > 10) {
				// bad things here...
				// abort();
				// global_var = 0;
				// memset(&global_var, 0, 10);
				// close(thread_pipefd[0]);  // -> +++ killed by SIGPIPE +++
				counter = 0;
			}


		} else {

			if (fds[0].revents & POLLIN) {
				int32_t message;	
				
				// TEMP_FAILURE_RETRY not needed -> signal durch poll -> non blocking
				err = TEMP_FAILURE_RETRY(read(threadData->unix_fd, &message, sizeof(int32_t)));
				assert( err == sizeof(int32_t));
				printf("Got message from loop main (%d)\n",message);
				if (message == 0) {
					printf("start thread...\n");
					timeout = 100;
				}

			}
		}
		
	}

	// when needed
	// free thread data -> memory leak! 
}


/**
 * Get a message from worker thread (non blocking function)
 * 
 * @param msg message object from thread
 * @return 0 means no item,   1 means one item
 */
static int  getMessagefromThread(int32_t *msg) {
	int err;

	err = read(socket_vector[0], msg, sizeof(int32_t));
	if (err == sizeof(int32_t))
		return 1; /* oder DEFINES e..g TRUE/FALSE/...*/

	if (err < 0 && errno == EAGAIN) {
		printf("Nix da\n");
		return 0;
	}

	// abort -> oder Rückmeldung - (Abhängig -> (fork)
	syslog(LOG_ERR, "In %s read failed with %d",__FUNCTION__,errno);	
	abort(); 

}


/**
 * Put a message from main loop to the  thread (non blocking function)
 * 
 * @param msg message object from thread
 * @return 0 means Ok, <0 means buffer full (e..g IDA_OK, ...)
 */
static int /*void?*/ putMessagetoThread(int32_t msg) {
	int err;

	
	err = write(socket_vector[0], &msg, sizeof(int32_t));
	if (err == sizeof(int32_t))
		return 0; /* IDA_OK */

	if (err < 0 && errno == EAGAIN) {
		printf("Der Puffer ist voll\n");
		return -1;  /* Zustand erlaubt ? z.B. 128 KByte Puffer*/
	}

	/* ENITR on O_NONBLOCK */
        // O_NONBLOCK blockiert nie, somit tritt nie ein EINTR auf
        // Behandlung dazu bringt nichts, stört aber auch nicht
	
	// abort -> oder Rückmeldung -
	syslog(LOG_ERR, "In %s write failed with %d",__FUNCTION__,errno);	
	abort(); 

}


/**
 * doLoop
 *
 */
void doLoop() {

	// Thread darf starten...

	putMessagetoThread(0 /* start message*/);


	while(1) {
		int32_t message;

		// Thread communication
#if 1
		if (getMessagefromThread(&message)) {

			printf("Got Message from Thread: %d (global_var: %d)\n", message, global_var);
			message += 1;

			putMessagetoThread(message);	// Behandlung überlauf

		}


#else
		/* Auslesen bis leer -> Auchtung auf Laufzeit in der Hauptschleife 
                 * Thread könnte weitere Daten parallel schreiben...
		 */

		// eventuell counter verwenden

		while(1 /* counter-- usw. */) {

			if (getMessagefromThread(&message)) {
				printf("Got Message from Thread: %d (global_var: %d)\n", message, global_var);
				message += 1;

				putMessagetoThread(message);	// Behandlung überlauf
			} else 
				break; // keine Nachricht mehr vorhanden
			

		}; 

#endif

		global_var++;
		
		usleep(15000);
	
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

	/** immer Man Page lesen!!! */
	err = socketpair(AF_UNIX, SOCK_SEQPACKET, 0 , socket_vector);
	assert(err == 0);  // note: Limits
	if (err < 0) {
		//syslog
		abort();
	}

	// allocate thread data	
	threadData_t *threaddata = ( threadData_t *) malloc(sizeof(threadData_t));
	threaddata->unix_fd = socket_vector[1];


	/** immer Man Page lesen!!! */
	err = pthread_create(&worker_thread, NULL, &doWorkerThread, threaddata); 
	
	assert(err == 0);	// Fatal -> wird sofort abgebrochen! -wie abort()

	if (err != 0) {	
		// interrnal error log  -> Soll uns Entwickler helfen! -
                // Achten auf LOG-Fluten !
		syslog(LOG_ERR, "In %s pthread_create failed with %d",__FUNCTION__,errno);
		return err;
	}  

		
	// set fd to  non blocking (e.g. control)
	err = fcntl( socket_vector[0], F_SETFL, fcntl(socket_vector[0], F_GETFL) | O_NONBLOCK);
	assert(err == 0);  
	if (err < 0) {
		//syslog
		abort();  // oder nur assert
	}


	return err;
}




