#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#define PASS 1
#define FAIL 0

int tls_create(unsigned int size);
int tls_destroy();
int tls_read(unsigned int offset, unsigned int length, char *buffer);
int tls_write(unsigned int offset, unsigned int length, char *buffer);
int tls_clone(pthread_t tid);

// used in several tests
//==============================================================================
static void* _thread_dummy(void* arg){
    while(1); //just wait forever
    return 0; //never actually return, just shutting the compiler up
}

int main()
{
    pthread_t tid1 = 0;

    // create a thread, to give anyone using their homegrown thread library a chance to init
    pthread_create(&tid1, NULL,  &_thread_dummy, NULL);

    char buf[8192];
    tls_create(8192);
    tls_read(0, 4, buf);
    tls_write(0, 4, buf);
    tls_destroy();

    return PASS;
}