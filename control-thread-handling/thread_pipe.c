/** 
 * Demo Code
 * Thread msg send to main loop
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

static int startWorker();

/**
 * Static objects
 */
static pthread_t worker_thread;
static volatile int thread_enabled = 0;
static int thread_pipefd[2];


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

	int32_t counter = 0;

	printf("Thread startes...\n");

	// oft findet man Warte Code 
	while (! thread_enabled) {   // mit einer Pipe nicht lösbar 
		sleep(1);
	}
	
	printf("Thread enabled...\n");


	while(1) {
		int err;

		// im Überlauf wird gewartet -> "Flow-Control"
		err = TEMP_FAILURE_RETRY(write(thread_pipefd[1], &counter, sizeof(int32_t)));
		assert(err); // Blocking write on overflow
		if (err < 0 ) {
			// -z.B. man 2 write
			abort();
		}


		usleep(50000); // overflow 50000 -> 1 zeigen
		
		if (++counter > 20) {
			// bad things here...
			// abort();
			// global_var = 0;
			// memset(&global_var, 0, 10);
			// close(thread_pipefd[0]);  // -> +++ killed by SIGPIPE +++
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
		int32_t message;

		// Thread communication
		err = read(thread_pipefd[0], &message, sizeof(int32_t));
		if (err == sizeof(int32_t)) {
			printf("Got message from thread: %d (global var: %d)\n", message, global_var);
		} else 
			if (err < 0 && errno == EAGAIN) {
				printf("Nix da\n");
		  	} else {
				// Error?!
				// Typisches syslog
				perror("read problem\n"); 
				abort();
			}
 

		global_var++;
		
		usleep(5000);
	
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
	err = pipe(thread_pipefd);
	assert(err == 0);  // note: Limits
	if (err < 0) {
		//syslog
		abort();
		return err;   
	}
		
	// set reader non blocking (e.g. control) -> keine Error-Abfrage im 2ten fcntl -> Nein!
	err = fcntl( thread_pipefd[0], F_SETFL, fcntl(thread_pipefd[0], F_GETFL) | O_NONBLOCK);
	assert(err == 0);  
	if (err < 0) {
		//syslog
		abort();  	// oder nur assert
		return err;   // oder weitergeben
	}



	/** immer Man Page lesen!!! */
	err = pthread_create(&worker_thread, NULL, &doWorkerThread, NULL); 
	
	assert(err == 0);	// Fatal -> wird sofort abgebrochen! -wie abort()

	if (err != 0) {	
		// interrnal error log  -> Soll uns Entwickler helfen! -
                // Achten auf LOG-Fluten !
		syslog(LOG_ERR, "In %s pthread_create failed with %d",__FUNCTION__,errno);
		return err;
	}  



	// Ignore Signal -> kein exit
	// Gilt für den ganzen Prozess !!!
	//  -> Allgemein Lösen
	sig_t signr = signal(SIGPIPE, SIG_IGN);
	assert( signr != SIG_ERR);

	return err;
}




