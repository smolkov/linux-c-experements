/** 
 * Demo Code
 * Process msg send/recv to main loop
 * Simple Demo -> Restart is missing (child die)
 */

#include <stdlib.h> 
#include <stdio.h>
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
#include <sys/wait.h>

static int startWorker(void);

/**
 * Static objects
 */
static int socket_vector[2];

/*
 * Global varibale
 */
int32_t global_var = 0;


/**
 * doWorkerProcess  Process
 *
 */
static void /* depends*/ doWorkerProcess(int unix_fd) {	
          
	
	int32_t counter = 0;
	struct pollfd fds[1];
	int timeout = -1; 	/* infinite timeout */

	// RLIMIT / close fd   
	// man setrlimit
	// set uid/gid...

	// work loop for process
	while(1) {

		int err;

		fds[0].fd = unix_fd;
		fds[0].events = POLLIN;

		// TEMP_FAILURE_RETRY -> EINTR (! -> nie vergessen)
		err = TEMP_FAILURE_RETRY(poll(fds, 1, timeout));

		if (err == -1) {
			/* should never fail -> man page */		
			syslog(LOG_ERR, "In %s poll failed with %d", __FUNCTION__, errno);	
			abort(); 					
		} 

		if (err == 0) {
				
			err = TEMP_FAILURE_RETRY(write(unix_fd, &counter, sizeof(int32_t)));
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
				//close(unix_fd);  
				//close(socket_vector[0]);
				//exit(1);
				counter = 0;
			}


		} else {

			if (fds[0].revents & POLLIN) {
				int32_t message;	
				
				// TEMP_FAILURE_RETRY not needed -> signal durch poll -> non blocking
				err = TEMP_FAILURE_RETRY(read(unix_fd, &message, sizeof(int32_t)));
				assert( err == sizeof(int32_t));
				printf("Got message from loop main (%d)\n",message);
				if (message == 0) {
					printf("start thread...\n");
					timeout = 100;
				}

			}
		}
		
	}

}


/**
 * Get a message from process  (non blocking function)
 * 
 * @param msg message object from thread
 * @return 0 means no item,   1 means one item
 */
static int  getMessage(int32_t *msg) {
	int err;

	err = read(socket_vector[0], msg, sizeof(int32_t));
	if (err == sizeof(int32_t))
		return 1; /* oder DEFINES e..g TRUE/FALSE/...*/

	if (err < 0 && errno == EAGAIN) {
		printf("Nix da\n");
		return 0;
	}

	//Abhängig -> (fork)  
	syslog(LOG_ERR, "In %s read failed with %d",__FUNCTION__,errno);	

	// Fehlerbehandlung  tbd

}


/**
 * Put a message from main loop to the process (non blocking function)
 * 
 * @param msg message object from process
 * @return 0 means Ok, <0 means buffer full (e..g IDA_OK, ...)
 */
static int /*void?*/ putMessage(int32_t msg) {
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

	// Fehlerbehandlung  tbd
	
}


/**
 * doLoop
 *
 */
void doLoop() {

	// Prozess darf starten...

	putMessage(0 /* start message*/);


	while(1) {
		int32_t message;

		// Process communication
#if 1
		if (getMessage(&message)) {

			printf("Got Message from Process: %d (global_var: %d)\n", message, global_var);
			message += 1;

			putMessage(message);	// Behandlung überlauf

		}


#else
		/* Auslesen bis leer -> Auchtung auf Laufzeit in der Hauptschleife 
                 * Process könnte weitere Daten parallel schreiben...
		 */

		// eventuell counter verwenden

		while(1 /* counter-- usw. */) {

			if (getMessage(&message)) {
				printf("Got Message from Process: %d (global_var: %d)\n", message, global_var);
				message += 1;

				putMessage(message);	// Behandlung überlauf
			} else 
				break; // keine Nachricht mehr vorhanden
			

		}; 

#endif

		global_var++;
		
		usleep(15000);
	
	}

}


int main() {

	pid_t child_pid;

	/* create process worker*/
	child_pid = startWorker();

	assert(child_pid != 0);

	/* example control loop */
	doLoop();


	return EXIT_SUCCESS;
}

/**
 * Demo signal handler
 */
static void signal_handler(int sig)
{
	pid_t pid;


	if (sig == SIGCHLD) {
		
		pid = wait(NULL);
		printf("**** Pid %d terminated *****\n", pid);

	}	
}


/**
 * Demo signal handler
 */
static pid_t startWorker() {

	pid_t child_pid;
	int err;

	/** immer Man Page lesen!!! */
	err = socketpair(AF_UNIX, SOCK_SEQPACKET, 0 , socket_vector);
	assert(err == 0);  // note: Limits
	if (err < 0) {
		//syslog
		abort();  
		// oder 0? -> Rückbehandlung
	}

	// set fd to non blocking (e.g. control)
	err = fcntl( socket_vector[0], F_SETFL, fcntl(socket_vector[0], F_GETFL) | O_NONBLOCK);
	assert(err == 0);  
	if (err < 0) {
		//syslog
		abort();  // oder nur assert
	}

	
	// Create child process	
	child_pid = fork();

	assert(child_pid >= 0);

	if (child_pid < 0) {   // Fork fail
		syslog(LOG_ERR, "In %s fork failed with %d", __FUNCTION__, errno);
		abort();  
		// oder 0? -> Rückbehandlung	

	}	

	
	if (child_pid == 0) {
		/* child process here */
		doWorkerProcess(socket_vector[1]);
		exit(EXIT_FAILURE);

	} 
	
	
	signal(SIGCHLD, signal_handler);


	/* parent */
	return child_pid;
}




