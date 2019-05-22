#include <tls.h>

using namespace std;

int tls_create(unsigned int size)
{
    /* Initialize the TLS Pool */
    if (!Initialized)
    {
    }

    /* Check for errors */
    /* The size has to be larger than 0 */
    if (size <= 0)
    {
        return -1;
    }
    /* Error to create a local storage for a thread that already has one */
    pthread_t tid = pthread_self();
    auto pred = [tid](const TLSBLOCK &item) {
        return item.ThreadID == tid;
    };
    auto iter = find_if(TLSPOOL.begin, TLSPOOL.end, pred);
    if (iter == TLSPOOL.end)
    {
        return -1;
    }
}

int tls_write(unsigned int offset, unsigned int length, char *buffer)
{
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
}

int tls_destroy()
{
}

int tls_clone(pthread_t tid)
{
}