#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data *data = (struct thread_data *)thread_param;

    data->thread_complete_success = false;

    // Wait before obtaining the mutex
    if (usleep(data->wait_to_obtain_ms * 1000) != 0) {
        return thread_param;
    }

    // Obtain the mutex
    if (pthread_mutex_lock(data->mutex) != 0) {
        return thread_param;
    }

    // Hold the mutex for the requested duration
    if (usleep(data->wait_to_release_ms * 1000) != 0) {
        pthread_mutex_unlock(data->mutex);
        return thread_param;
    }

    // Release the mutex
    if (pthread_mutex_unlock(data->mutex) != 0) {
        return thread_param;
    }

    data->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread,
                                  pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms,
                                  int wait_to_release_ms)
{
    // Allocate memory for the thread arguments
    struct thread_data *data = malloc(sizeof(struct thread_data));

    if (data == NULL) {
        ERROR_LOG("Failed to allocate thread data");
        return false;
    }

    // Initialize the thread arguments
    data->mutex = mutex;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
    data->thread_complete_success = false;

    // Create the thread
    int result = pthread_create(thread, NULL, threadfunc, data);

    if (result != 0) {
        ERROR_LOG("Failed to create thread");
        free(data);
        return false;
    }

    return true;
}

