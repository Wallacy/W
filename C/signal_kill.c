#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>

 // set up mask

#define MSG "SIGUSR1 received\n"

void handle_SIGUSR1(int sig)
{
    write(STDOUT_FILENO, MSG, strlen(MSG));
}

void * reporter(void *arg)
{
    printf("Entered reporter thread\n");
    
    struct sigaction sa = {0};
    sa.sa_handler = handle_SIGUSR1; 
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) < 0) {
        perror("sigaction");
        return NULL;
    }

    sigset_t new_sig_set;
    sigemptyset(&new_sig_set); // empty the mask
    sigaddset(&new_sig_set, SIGUSR1); // add SIGUSR1 to the mask
    int err;
    if ((err = pthread_sigmask(SIG_UNBLOCK, &new_sig_set, NULL)) != 0) {
        fprintf(stderr, "pthread_sigmask: %s", strerror(err));
        return NULL;
    }
    while(1){
        pause();
    }
}

int main(int argc, char **argv)
{
    sigset_t new_sig_set;
    sigemptyset(&new_sig_set); // empty the mask
    sigaddset(&new_sig_set, SIGUSR1); // add SIGUSR1 to the mask
    int err;
    if ((err = pthread_sigmask(SIG_BLOCK, &new_sig_set, NULL)) != 0) {
        fprintf(stderr, "pthread_sigmask: %s", strerror(err));
        exit(1);
    }

    int pid = fork();
    if (pid == 0)
    {
        pthread_t reporter_thread;
        int err;

        if ((err = pthread_create(&reporter_thread, NULL, reporter, NULL)) != 0) {
            fprintf(stderr, "pthread_create: %s", strerror(err));
            exit(1);
        }
        printf("Created reporter thread\n");
        fflush(stdout);
        pthread_join(reporter_thread, NULL); // never returns
    }
    else if (pid > 0)
    {
        srand((unsigned)time(NULL));
        for (int i = 0; i < 5; i++)
        {
            printf("Generating signal\n");
            fflush(stdout);
            if (kill(pid, SIGUSR1) < 0)
                perror("kill");
            sleep(1);
        }
        kill(pid, SIGTERM); // kill it for reals
        return 0;
    }
    else
    {
        perror("fork");
    }
}
