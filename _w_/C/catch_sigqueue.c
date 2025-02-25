#include <signal.h>
#include <unistd.h>
#include <stdio.h>

void act(int s, siginfo_t *i, void *v)
{
    //printing from here is unsafe but this is for testing 
    //purposes only
    printf("pid=%d val=%d\n", i->si_pid, i->si_value.sival_int);
}
int main(int argc, char **argv)
{
    struct sigaction sa = { 
        .sa_sigaction=act ,
        .sa_flags = SA_RESTART|SA_SIGINFO
    } ;
    sigaction(SIGUSR1, &sa, NULL);
    printf("%d\n", (int)getpid());
    for(;;){
        pause();
    }

}