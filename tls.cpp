#include "tls.h"

using namespace std;

void tls_init()
{
    PAGESIZE = getpagesize();

    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);

    /* SA_SIGINFO will help to distinguish between page fault and normal SegFault */
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = tls_handle_page_fault;
    sigaction(SIGBUS, &sigact, NULL);
    sigaction(SIGSEGV, &sigact, NULL);

    Initialized = 1;
}

void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
    unsigned long p_fault = ((unsigned long)si->si_addr) & ~(PAGESIZE - 1);

    /* Check whether it is a "real" segfault or because a thread has touched forbidden memory */
    /* Make a brute force scan through all allocated TLS regions and pages in each TLS */
    for (auto const &tlsblock : TLSPOOL)
    {
        for (int j = 0; j < tlsblock.page_num; j++)
        {
            if (tlsblock.pages[j]->address == p_fault)
            {
                tls_destroy();
                pthread_exit(NULL);
            }
        }
    }

    /* If not page fault, set up a normal Seg Fault */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig);
}

int tls_create(unsigned int size)
{
    pthread_t tid = pthread_self();

    /* Initialize the TLS Pool */
    if (!Initialized)
    {
        tls_init();
    }

    /* Check for errors */
    /* The size has to be larger than 0 */
    if (size <= 0)
    {
        return -1;
    }
    /* Error to create a local storage for a thread that already has one */
    auto iter = hash_table.find(tid);
    if (iter != hash_table.end())
    {
        return -1;
    }

    /* Allocate TLS */
    struct TLSBLOCK tls;
    tls.tid = tid;
    tls.size = size;
    tls.page_num = (size - 1) / PAGESIZE + 1;
    tls.pages = (struct page **)calloc(tls.page_num, sizeof(struct page));

    /* Allocate all pages for this TLS */
    for (int i = 0; i < tls.page_num; i++)
    {
        struct page *p;
        p = (struct page *)calloc(1, sizeof(struct page));
        p->address = (unsigned long)mmap(NULL, PAGESIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        p->ref_count = 1;
        tls.pages[i] = p;
    }

    /* Add this TLS to the TLSPOOL */
    TLSPOOL.push_back(tls);

    /* Add this threadid and TLS mapping to flobal hash_table */
    hash_table[tid] = TLSPOOL.size() - 1;

    return 0;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t tid = pthread_self();

    /* Check whether current thread already has local storage */
    auto iter = hash_table.find(tid);
    if (iter == hash_table.end())
    {
        return -1;
    }
    auto tls_iter = next(TLSPOOL.begin(), iter->second);

    /* Check if offset+length > size */
    if (offset + length > tls_iter->size)
    {
        return -1;
    }

    /* Unprotect all pages belong to thread's TLS */
    for (int i = 0; i < tls_iter->page_num; i++)
    {
        mprotect((void *)tls_iter->pages[i]->address, PAGESIZE, PROT_READ | PROT_WRITE);
        // tls_unprotect(tls_iter->pages[i]);
    }

    /* perform the write operation */
    for (int i = 0, index = offset; index < (offset + length); i++, index++)
    {
        struct page *p;
        unsigned int pn, poff;
        pn = index / PAGESIZE;
        poff = index % PAGESIZE;
        p = tls_iter->pages[pn];
        /* If this page is shared */
        if (p->ref_count > 1)
        {
            struct page *copy;

            /* Create a private copy (COW) */
            copy = (struct page *)calloc(1, sizeof(struct page));
            copy->address = (unsigned long)mmap(NULL, PAGESIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
            copy->ref_count = 1;
            tls_iter->pages[pn] = copy;

            /* Update original page */
            p->ref_count--;
            mprotect((void *)p->address, PAGESIZE, 0);
            p = copy;

            /* Unprotect the newly allocated mmap page */
            mprotect((void *)p->address, PAGESIZE, PROT_READ | PROT_WRITE);
        }

        char *dst = ((char *)p->address) + poff;
        *dst = buffer[i];
    }

    /* Reprotect all pages belong to thread's TLS */
    for (int i = 0; i < tls_iter->page_num; i++)
    {
        mprotect((void *)tls_iter->pages[i]->address, PAGESIZE, 0);
    }

    return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
    pthread_t tid = pthread_self();

    /* Check whether current thread already has local storage */
    auto iter = hash_table.find(tid);
    if (iter == hash_table.end())
    {
        return -1;
    }
    auto tls_iter = next(TLSPOOL.begin(), iter->second);

    /* Check if offset+length > size */
    if (offset + length > tls_iter->size)
    {
        return -1;
    }

    /* Unprotect all pages belong to thread's TLS */
    for (int i = 0; i < tls_iter->page_num; i++)
    {
        mprotect((void *)tls_iter->pages[i]->address, PAGESIZE, PROT_READ | PROT_WRITE);
    }

    /* Perform the read operation */
    for (int i = 0, index = offset; index < (offset + length); i++, index++)
    {
        struct page *p;
        unsigned int pn, poff;
        pn = index / PAGESIZE;
        poff = index % PAGESIZE;
        p = tls_iter->pages[pn];
        char *src = ((char *)p->address) + poff;
        buffer[i] = *src;
    }

    /* Reprotect all pages belong to thread's TLS */
    for (int i = 0; i < tls_iter->page_num; i++)
    {
        mprotect((void *)tls_iter->pages[i]->address, PAGESIZE, 0);
    }

    return 0;
}

int tls_destroy()
{
    pthread_t tid = pthread_self();

    /* Check whether current thread has local storage for destroying */
    auto iter = hash_table.find(tid);
    if (iter == hash_table.end())
    {
        return -1;
    }
    auto tls_iter = next(TLSPOOL.begin(), iter->second);

    /* Clean up all pages */
    for (int i = 0; i < tls_iter->page_num; i++)
    {
        /* If the page is not shared by other threads */
        if (tls_iter->pages[i]->ref_count == 1)
        {
            munmap((void *)tls_iter->pages[i]->address, PAGESIZE);
            free(tls_iter->pages[i]);
        }
        /* If the page is shared by other threads */
        else
        {
            /* Decrement the reference counter by one */
            tls_iter->pages[i]->ref_count--;
        }
    }

    /* Update the index in the hash_table for elements after tls_iter in the list  */
    auto update_iter = next(TLSPOOL.begin(), iter->second);
    update_iter++;
    for (update_iter; update_iter != TLSPOOL.end(); ++update_iter)
    {
        auto hash_iter = hash_table.find(update_iter->tid);
        hash_iter->second--;
    }

    /* Clean up TLS */
    free(tls_iter->pages);
    TLSPOOL.erase(tls_iter);

    /* Remove the mapping from hash_table */
    hash_table.erase(iter);

    return 0;
}

int tls_clone(pthread_t tid)
{
    pthread_t tid_self = pthread_self();

    /* Check whether current thread has local storage */
    auto iter2 = hash_table.find(tid_self);
    if (iter2 != hash_table.end())
    {
        return -1;
    }

    /* Check whether target thread has local storage */
    auto iter = hash_table.find(tid);
    if (iter == hash_table.end())
    {
        return -1;
    }
    auto tls_iter = next(TLSPOOL.begin(), iter->second);

    /* Allocate TLS */
    struct TLSBLOCK tls;
    tls.tid = tid_self;
    tls.size = tls_iter->size;
    tls.page_num = tls_iter->page_num;
    tls.pages = (struct page **)calloc(tls.page_num, sizeof(struct page));

    /* Allocate all pages for this TLS */
    for (int i = 0; i < tls.page_num; i++)
    {
        tls.pages[i] = tls_iter->pages[i];
        tls.pages[i]->ref_count++;
    }

    /* Add this TLS to the TLSPOOL */
    TLSPOOL.push_back(tls);

    /* Add this threadid and TLS mapping to flobal hash_table */
    hash_table[tid_self] = TLSPOOL.size() - 1;

    return 0;
}