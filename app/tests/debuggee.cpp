#include <cstdio>
#include <thread>
#include <chrono>
#ifdef __linux__
#include <sys/prctl.h>
#endif
int increment(int value) {
    volatile int next = value + 1;
    return next;
}
int main(int argc, char **) {
    if (argc > 1) {
#ifdef __linux__
        prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY); // Owned acceptance fixture only.
#endif
        while (true) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    int value = 41;
    value = increment(value); // BREAK_HERE
    std::printf("RESULT=%d\n", value);
    return value == 42 ? 0 : 1;
}
