#include <iostream>
#include <csignal>
#include <cassert>
#include <chrono>

#define TIME_SECS 10
volatile bool timeout = false;

static void timer_handle(int sig);
static void swap_head_tail(char *p, const size_t len);

static void 
timer_handle(int sig) {
    if (sig == SIGALRM)
        timeout = true;
}

static void 
swap_head_tail(char *p, const size_t len) {
    char head = p[0];
    char tail = p[len-1];
    p[0] = tail;
    p[len-1] = head;
}

int main() {
    signal(SIGALRM, timer_handle);

    const size_t len = 1024;
    long long count = 0;

    alarm(TIME_SECS);
    auto start = std::chrono::high_resolution_clock::now();
    while (!timeout) {
        char *p = new char[len];
        assert(p != nullptr);
        swap_head_tail(p, len);
        delete[] p;
        ++count;
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = end - start;
    auto ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    std::cout << "count = " << count << ", ms_count = " << ms_count << 
        ", per ms = " << count/ms_count << std::endl;

    return 0;
}