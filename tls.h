#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <list>
#include <map>

using namespace std;

#define MAX_TLS 129

void tls_init();

void tls_handle_page_fault(int sig, siginfo_t *si, void *context);

int tls_create(unsigned int size);

int tls_write(unsigned int offset, unsigned int length, char *buffer);

int tls_read(unsigned int offset, unsigned int length, char *buffer);

int tls_destroy();

int tls_clone(pthread_t tid);

void* tls_get_internal_start_address();

struct TLSBLOCK
{
    pthread_t tid = 0;
    unsigned int size = 0;     /* Size in bytes */
    unsigned int page_num = 0; /* Number of pages */
    struct page **pages;       /* Array of pointers to pages */
};

struct page
{
    unsigned long address; /* Start address of page */
    int ref_count;         /* Counter for shared pages */
};

map<pthread_t, int> hash_table;

static list<TLSBLOCK> TLSPOOL;

static int Initialized = 0;

static int PAGESIZE = 0;

sem_t mutex_sem;