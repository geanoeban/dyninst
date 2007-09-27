#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mutatee_util.h"

#if !defined(os_windows)
#include <unistd.h>
#else
#include <windows.h>
#endif

#define NTHRD 4

volatile int done;
volatile int proc_current_state;
volatile int threads_running[NTHRD];

void *init_func(void *arg)
{
   threads_running[(int) (long) arg] = 1;
   while (!done);
   return arg;
}

/* int main(int argc, char *argv[]) */
int test_thread_6_mutatee() {
   unsigned i;
   thread_t threads[NTHRD];
   int startedall = 0;

#ifndef os_windows
   char c = 'T';
#endif

   /* *DEBUG* */
   /*fprintf(stderr, "test_thread_6.mutatee: in main()\n"); /* *DEBUG* */

   /* initThreads() has an empty function body? */
   initThreads();
   /* fprintf(stderr, "test13.mutatee: initialized threads\n"); /* *DEBUG* */

   for (i=0; i<NTHRD; i++)  {
       threads[i] = spawnNewThread((void *) init_func, (void *) i);
   }
   /* fprintf(stderr, "test13.mutatee: spawned threads\n"); /* *DEBUG* */
   
   while (!startedall) {
      for (i=0; i<NTHRD; i++) {
         startedall = 1;
         if (!threads_running[i]) {
            startedall = 0;
            P_sleep(1);
            break;
         }
      }
   }
   /* fprintf(stderr, "test13.mutatee: threads now running\n"); /* *DEBUG* */

   /* Hmm..  So in attach mode we create all the threads before waiting for
    * the mutator to attach, but in create mode we (by necessity) create all
    * the threads after the mutator has attached (and let us run..)
    * And this test runs in both create and attach modes...
    * I'm going to strip out this attach mode handling and see if it works
    * alright when I let the mutatee driver handle it.
    */

#if 0
#ifndef os_windows
   if (attached_fd) {
      /* fprintf(stderr, "test13.mutatee: writing to pipe\n"); /* *DEBUG* */
      if (write(attached_fd, &c, sizeof(char)) != sizeof(char)) {
         fprintf(stderr, "*ERROR*: Writing to pipe\n");
         exit(-1);
      }
      /* fprintf(stderr, "test13.mutatee: closing pipe\n"); /* *DEBUG* */
      close(attached_fd);
      /* fprintf(stderr, "test13.mutatee: closed pipe\n"); /* *DEBUG* */
      while (!checkIfAttached()) {
         usleep(1);
      }
   }
   /* fprintf(stderr, "test13.mutatee: wrote to pipe\n"); /* *DEBUG* */
#else
   if (attached_fd)
      while (!checkIfAttached());
#endif
#endif

   /* fprintf(stderr, "test13.mutatee: logging status\n"); /* *DEBUG* */
   logstatus("[%s:%d]: stage 1 - all threads created\n", __FILE__, __LINE__);
   /* fprintf(stderr, "test13.mutatee: logged status\n"); /* *DEBUG* */
   while (proc_current_state == 0) {
     /* Do nothing */
   }
   logstatus("[%s:%d]: stage 2 - allowing threads to exit\n", __FILE__, __LINE__);
   done = 1;
   for (i=0; i<NTHRD; i++)
   {
      joinThread(threads[i]);
   }
   logstatus("[%s:%d]: stage 3 - all threads joined\n", __FILE__, __LINE__);
   /* Is the return value of this mutatee checked?  Doesn't look like it */
   return 0;
}