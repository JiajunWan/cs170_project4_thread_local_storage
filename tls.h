#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <list>

using namespace std;

int tls_create(unsigned int size);

int tls_write(unsigned int offset, unsigned int length, char *buffer);

int tls_read(unsigned int offset, unsigned int length, char *buffer);

int tls_destroy();

int tls_clone(pthread_t tid);

struct TLSBLOCK
{
    pthread_t ThreadID;
    int *TLS = NULL;
    int ListIndex = -1;
};

static list<TLSBLOCK> TLSPOOL;

static int Initialized = 0;