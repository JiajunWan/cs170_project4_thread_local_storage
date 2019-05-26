/**
 * Sean Gillen April 2019
 *
 * This file will automatically grade your cs170 project 4.
 * Please see the README for more info
 */


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

#define NUM_TESTS 8
#define PASS 1
#define FAIL 0

#define TEST_WAIT_MILLI 2000 // how many milliseconds do we wait before assuming a test is hung


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




// if your code compiles you pass test 0 for free
//==============================================================================
static int test0(void){
    return PASS;
}


// another gimme, can you call all your functions without crashing
//==============================================================================

static int test1(void){
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


// basic read / write functionality
//==============================================================================
static int test2(void){
    pthread_t tid1 = 0;

    // create a thread, to give anyone using their homegrown thread library a chance to init
    pthread_create(&tid1, NULL,  &_thread_dummy, NULL);

    char in_buf[4] = {1,1,1,1};
    char out_buf[4] = {0};

    tls_create(8192);
    tls_write(0, 4, in_buf);
    tls_read(0, 4, out_buf);

    for(int i = 0; i < 4; i++){
        if(out_buf[i] != 1){
            return FAIL;
        }
    }

    return PASS;
}

// clone test 1
//==============================================================================
static void* _thread_create(void* arg){
    char in_buf[4] = {2,2,2,2};
    sem_t* mutex_sem = (sem_t*)arg;

    tls_create(8192);
    tls_write(0, 4, in_buf);
    sem_post(mutex_sem);

    while(1);
    return 0;
}


static int test3(void){
    pthread_t tid1 = 0;
    sem_t mutex_sem;
    char out_buf[4] = {0};

    // init sem, call thread, and wait until thread 2 creates and writes to it's LSA
    sem_init(&mutex_sem, 0, 0);
    pthread_create(&tid1, NULL,  &_thread_create, &mutex_sem);
    sem_wait(&mutex_sem);

    // clone thread 2's LSA and make sure the values read are what we put there
    tls_clone(tid1);
    tls_read(0, 4, out_buf);

    for(int i = 0; i < 4; i++){
        if(out_buf[i] != 2){
            return FAIL;
        }
    }

    return PASS;
}

// clone test 2
//==============================================================================
static void* _thread_create2(void* arg){
    char in_buf[4] = {2,2,2,2};
    sem_t* mutex_sems = (sem_t*)arg;

    // create new LSA and signal to thread 1 when done
    tls_create(8192);
    sem_post(&mutex_sems[0]);

    // wait for thread 1 to clone, write our own values, then signal back to thread 1
    sem_wait(&mutex_sems[1]);
    tls_write(0, 4, in_buf);
    sem_post(&mutex_sems[0]);

    while(1); // just wait forever, don't want to exit and release LSA memory
    return 0; // shut the compiler up
}


static int test4(void){
    pthread_t tid1 = 0;
    sem_t mutex_sems[2];
    char in_buf[4] = {1,1,1,1};
    char out_buf[4] = {0,0,0,0};


    // init semaphores and wait for thread 2 to create LSA
    sem_init(&mutex_sems[0], 0, 0);
    sem_init(&mutex_sems[1], 0, 0);
    pthread_create(&tid1, NULL,  &_thread_create2, mutex_sems);
    sem_wait(&mutex_sems[0]);

    // clone and write our own new values, and signal to thread 2 it's time to write values
    tls_clone(tid1);
    tls_write(0, 4, in_buf);
    sem_post(&mutex_sems[1]);

    // wait until thread 2 writes, then read out the LSA
    sem_wait(&mutex_sems[0]);
    tls_read(0, 4, out_buf);

    // make sure our data was not overwritten by thread 2
    for(int i = 0; i < 4; i++){
        if(out_buf[i] != 1){
            return FAIL;
        }
    }

    return PASS;
}


// destroy test 1
//==============================================================================
static void* _thread_destroy(void* arg){
    char in_buf[4] = {2,2,2,2};
    sem_t* mutex_sems = (sem_t*)arg;

    // create new LSA and signal to thread 1 when done
    tls_create(8192);
    sem_post(&mutex_sems[0]);

    // wait for thread 1 to clone, then destroy
    sem_wait(&mutex_sems[1]);
    tls_destroy();
    sem_post(&mutex_sems[0]);

    while(1); // just wait forever, don't want to exit and release LSA memory
    return 0; // shut the compiler up
}


static int test5(void){
    pthread_t tid1 = 0;
    sem_t mutex_sems[2];
    char in_buf[4] = {1,1,1,1};
    char out_buf[4] = {0,0,0,0};

    // init semaphores and wait for thread 2 to create LSA
    sem_init(&mutex_sems[0], 0, 0);
    sem_init(&mutex_sems[1], 0, 0);
    pthread_create(&tid1, NULL,  &_thread_destroy, mutex_sems);
    sem_wait(&mutex_sems[0]);

    // clone and write our own new values, and signal to thread 2 it's time to write values
    tls_clone(tid1);
    tls_write(0, 4, in_buf);
    sem_post(&mutex_sems[1]);

    // wait until thread 2 calls destroy, then read out the LSA
    sem_wait(&mutex_sems[0]);
    tls_read(0, 4, out_buf);

    // make sure we don't die because of accessing freed memory
    for(int i = 0; i < 4; i++){
        if(out_buf[i] != 1){
            return FAIL;
        }
    }

    return PASS;
}


// COW performance test
//==============================================================================
static void* _thread_clone_timed(void* arg){
    char in_buf[4] = {2,2,2,2};
    sem_t* mutex_sem = (sem_t*)arg;

    tls_create(4096*1024);
    sem_post(mutex_sem);

    while(1);
    return 0;
}


int test6(void){
    pthread_t tid1 = 0;
    sem_t mutex_sem;
    char in_buf[4] = {1,1,1,1};

    //init sem, call thread, and wait until thread 2 creates and writes to it's LSA
    sem_init(&mutex_sem, 0, 0);
    pthread_create(&tid1, NULL,  &_thread_clone_timed, &mutex_sem);
    sem_wait(&mutex_sem);


    clock_t begin = clock();

    tls_clone(tid1);
    tls_write(0, 4, in_buf);

    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

    // empirically on CSIL time_spent with COW  = 0.001650
    // time spent with naive copy = 0.026228
    // we decided to give you the student .01 seconds

    if (time_spent < .01){
        return PASS;
    }else{
        return FAIL;
    }
}


// some basic error tests, don't forget to check the spec
//==============================================================================
void* _thread_create_dummy(void* arg){
    tls_create(8);
    tls_destroy();
    pthread_exit(0);
}


int test7(void){
    pthread_t tid1 = 0;
    char in_buf[4] = {1,1,1,1};

    // allow everyone a chance to init everything
    pthread_create(&tid1, NULL,  &_thread_create_dummy, NULL);
    pthread_join(tid1, NULL);

    // should return -1 because haven't initialized tls
    if(tls_write(0, 4, in_buf) == 0){
        return FAIL;
    }

    tls_create(2);
    if(tls_write(0, 4, in_buf) == 0){
        return FAIL;
    }

    return PASS;
}



/**
 *  Some implementation details: Main spawns a child process for each
 *  test, that way if test 2/20 segfaults, we can still run the remaining
 *  tests. It also hands the child a pipe to write the result of the test.
 *  the parent polls this pipe, and counts the test as a failure if there
 *  is a timeout (which would indicate the child is hung).
 */


// Yeah I don't exactly love this either
static int (*test_arr[NUM_TESTS])(void) = {&test0, &test1, &test2,
                                           &test3, &test4, &test5,
                                           &test6, &test7};

int main(void){

    int status; pid_t pid;
    int pipe_fd[2]; struct pollfd poll_fds;
    int score = 0; int total_score = 0;

    int devnull_fd = open("/dev/null", O_WRONLY);

    pipe(pipe_fd);
    poll_fds.fd = pipe_fd[0]; // only going to poll the read end of our pipe
    poll_fds.events = POLLRDNORM; // only care about normal read operations

    for(int i = 0; i < NUM_TESTS; i++){
        score = 0;
        pid = fork();

        // child, launches the test
        if (pid == 0){
            dup2(devnull_fd, STDOUT_FILENO); //begone debug messages
            dup2(devnull_fd, STDERR_FILENO);

            score = test_arr[i]();

            write(pipe_fd[1], &score, sizeof(score));
            exit(0);
        }

            // parent, polls on the pipe we gave the child, kills the child,
            // keeps track of score
        else{

            if(poll(&poll_fds, 1, TEST_WAIT_MILLI)){
                read(pipe_fd[0], &score, sizeof(score));
            }

            total_score += score;
            kill(pid, SIGKILL);
            waitpid(pid,&status,0);


            if(score){
                printf("test %i : PASS\n", i);
            }
            else{
                printf("test %i : FAIL\n", i);
            }
        }
    }

    printf("total score was %i / %i\n", total_score, NUM_TESTS);
    return 0;
}