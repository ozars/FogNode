#ifndef _6390_PROJECT_DEBUG_H_
#define _6390_PROJECT_DEBUG_H_

#ifdef DEBUG

#include <err.h>
#include <errno.h>

#include <cstdlib>
#include <ctime>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

using namespace std;

#define DRAW(x)  do { cerr << x; } while(0)
#define DINFO(x)  do { _debug_print_timestamp(cerr); cerr << "[INFO@" << __FUNCTION__ << ":" << __LINE__ << "]\t" << x << endl; } while(0)
#define DERROR(x, retval) do { _debug_print_timestamp(cerr); cerr << "[ERROR@" << __LINE__ << "]\t" << x << endl; exit(retval); } while(0)
#define EXIT_ON_ERROR(expr) _debug_exit_on_error(expr, __FILE__, __LINE__)

void inline _debug_exit_on_error(int retval, const char* filename, int line)
{
    if(retval < 0) {
        err(1, "[%s:%d] Function returned %d with errno %d", filename, line, retval, errno);
    }
}

void inline _debug_print_timestamp(ostream& is)
{
    using namespace std;
    using namespace std::chrono;
    
    auto now = high_resolution_clock::now();
    auto tt = high_resolution_clock::to_time_t(now);
    auto local_tm = *localtime(&tt);
    is  << "["
        << setfill('0') << setw(2) << local_tm.tm_hour << ":"
        << setfill('0') << setw(2) << local_tm.tm_min << ":"
        << setfill('0') << setw(2) << local_tm.tm_sec << "."
        << setfill('0') << setw(3) << duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000
        << "." << setfill('0') << setw(3) << duration_cast<microseconds>(now.time_since_epoch()).count() % 1000
        //<< "." << setfill('0') << setw(3) << duration_cast<nanoseconds>(now.time_since_epoch()).count() % 1000
        << "]";
}

#else
#define DINFO(x)  do {} while(0)
#define DERROR(x, retval) exit(-1)
#define EXIT_ON_ERROR(expr) _exit_on_error(expr);
#endif

void inline _exit_on_error(int retval)
{
    if(retval < 0) {
        err(1, "Some error occured: ");
    }
}

#endif /* _6390_PROJECT_DEBUG_H_ */
