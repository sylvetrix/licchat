#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>

#define DAEMON_NAME	"licchatd"
#define DAEMON_PATH	"/tmp/licchatd/"
#define DAEMON_LOCK	"/tmp/licchatd/licchat.pid"
#define DAEMON_LOG 	"/tmp/licchatd/licchat.log"

void daemonize(char *rundir, char *pidfile);
void daemonShutdown();
void signalHandler(int sig);

int pidFileHandle;

int main(int argc, char **argv)
{
	// debug logging
	//setlogmask(LOG_UPTO(LOG_DEBUG));
	//openlog(DAEMON_NAME, LOG_CONS, LOG_USER);

	// logging
	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	syslog(LOG_INFO, "Daemon is starting up");

	// daemonize
	daemonize(DAEMON_PATH, DAEMON_LOCK);

	syslog(LOG_INFO, "Daemon running");

	while (1)
	{
		sleep(1);
	}
}

void daemonize(char *rundir, char *pidfile)
{
	int pid, sid, i;
	char str[10];
	struct sigaction newSigAction;
	sigset_t newSigSet;

	// check if parent pid is set
	if (getppid() == 1)
	{
		// ppid existsm so daemon is already running
		return;
	}

	// set signal mask (signals to block)
	sigemptyset(&newSigSet);
	sigaddset(&newSigSet, SIGCHLD);	// ignore child
	sigaddset(&newSigSet, SIGTSTP);	// ignore TTY stop signals
	sigaddset(&newSigSet, SIGTTOU);	// ignore TTY background writes
	sigaddset(&newSigSet, SIGTTIN);	// ignore TTY background reads
	sigprocmask(SIG_BLOCK, &newSigSet, NULL);	// block above signals

	// set up signal handler
	newSigAction.sa_handler = signalHandler;
	sigemptyset(&newSigAction.sa_mask);
	newSigAction.sa_flags = 0;

	// signals to handle
	sigaction(SIGHUP, &newSigAction, NULL);	// catch hangup signal
	sigaction(SIGTERM, &newSigAction, NULL);	// catch term signal
	sigaction(SIGINT, &newSigAction, NULL);	// catch interrupt signal

	// fork
	pid = fork();

	if (pid < 0)
	{
		// could not fork
		exit(EXIT_FAILURE);
	}

	if (pid > 0)
	{
		// child created ok, so exit parent process
		printf("Child process created: %d\n", pid);
		exit(EXIT_SUCCESS);
	}

	// child continues

	umask(027);	// set file permissions to 750

	// get new process group
	sid = setsid();

	if (sid < 0)
	{
		exit(EXIT_FAILURE);
	}

	// close all descriptors
	for (i = getdtablesize(); i >= 0; i--)
	{
		close(i);
	}

	// route I/O connections

	// open STDIN
	i = open("/dev/null", O_RDWR);

	// open STDOUT
	dup(i);

	// open STDERR
	dup(i);

	// change running directory
	chdir(rundir);

	// ensure only one copy
	pidFileHandle = open(pidfile, O_RDWR|O_CREAT, 0600);

	if (pidFileHandle == -1)
	{
		// couldn't open lock file
		syslog(LOG_INFO, "Could not open PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	if (lockf(pidFileHandle, F_TLOCK, 0) == -1)
	{
		// couldn't get a lock on the lock file
		syslog(LOG_INFO, "Could not lock PID lock file %s, exiting", pidfile);
		exit(EXIT_FAILURE);
	}

	// get and format pid
	sprintf(str, "%d\n", getpid());

	// write pid to lock file
	write(pidFileHandle, str, strlen(str));
}

void daemonShutdown()
{
	close(pidFileHandle);
}

void signalHandler(int sig)
{
	switch (sig)
	{
		case SIGHUP:
			syslog(LOG_WARNING, "Received SIGHUP signal");
			break;
		case SIGINT:
		case SIGTERM:
			syslog(LOG_INFO, "Daemon exiting");
			daemonShutdown();
			exit(EXIT_SUCCESS);
			break;
		default:
			syslog(LOG_WARNING, "Unhandled signal %s", strsignal(sig));
			break;
	}
}

