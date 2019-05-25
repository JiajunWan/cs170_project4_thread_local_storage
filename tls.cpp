#include <tls.h>

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
    int p_fault = ((unsigned int)si->si_addr) & ~(PAGESIZE - 1);

    /* 1. check whether it is a "real" segfault or because a thread has touched forbidden memory
        a. make a brute force scan through all allocated TLS regions and pages in each TLS
        b. if (pageaddress == p_fault) // pageaddress in bruteforce scan
            pthread_exit(NULL); */
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

    /* 2. If not page fault, i.e; its a normal Seg Fault */
    signal(SIGSEGV, SIG_DFL);
    signal(SIGBUS, SIG_DFL);
    raise(sig);
}

void tls_protect(struct page *p)
{
    if (mprotect((void *)p->address, PAGESIZE, 0))
    {
        fprintf(stderr, "tls_protect: could not protect page\n");
        exit(1);
    }
}

void tls_unprotect(struct page *p)
{
    if (mprotect((void *)p->address, PAGESIZE, PROT_READ | PROT_WRITE))
    {
        fprintf(stderr, "tls_unprotect: could not unprotect page\n");
        exit(1);
    }
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
    // auto pred = [tid](const TLSBLOCK &tlsblock) {
    //     return tlsblock.ThreadID == tid;
    // };
    // auto iter = find_if(TLSPOOL.begin, TLSPOOL.end, pred);
    // if (iter != TLSPOOL.end)
    // {
    //     return -1;
    // }
    auto iter = hash_table.find(tid);
    if (iter != TLSPOOL.end)
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
        p->address = (unsigned int)mmap(NULL, PAGESIZE, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        p->ref_count = 1;
        tls.pages[i] = p;
    }

    /* Add this TLS to the TLSPOOL */
    TLSPOOL.push_back(tls);

    /* Add this threadid and TLS mapping to flobal hash_table */
    hash_table[tid] = TLSPOOL.size - 1;
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
}

int tls_destroy()
{
    pthread_t tid = pthread_self();

    /* Error handling: Check whether current thread has local storage for destroying */
    auto iter = hash_table.find(tid);
    if (iter == TLSPOOL.end)
    {
        return -1;
    }

    auto it1 = next(TLSPOOL.begin(), iter->second);

    /* Clean up all pages */
    for (int i = 0; i < it1->page_num; i++)
    {
        /* If the page is not shared by other threads */
        if (it1->pages[i]->ref_count == 1)
        {
            munmap((void *)it1->pages[i]->address, PAGESIZE);
            free(it1->pages[i]);
        }
        /* If the page is shared by other threads */
        else
        {
            /* Decrement the reference counter by one */
            it1->pages[i]->ref_count--;
        }
    }

    /* Clean up TLS */
    free(it1->pages);
    TLSPOOL.erase(it1);

    /* Remove the mapping from hash_table */
    hash_table.erase(iter);

    return 0;
}

int tls_clone(pthread_t tid)
{
}