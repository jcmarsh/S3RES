/*
 * Meant to stress the cpu and record it's resource usage.
 * Supposed to be based off of sensor data... so it will have range / pose updates.
 *
 * James Marshall
 */

#include "controller.h"

#include <png.h>
#include <ccv.h>

#include <sys/time.h>
#include <sys/resource.h>

#define PIPE_COUNT 1

struct typed_pipe pipes[PIPE_COUNT];
int pipe_count = PIPE_COUNT;
int out_to_log;

// TAS related
int priority;
int pinned_cpu;

const char* name = "Load";

bool insertCFE = false;
bool insertSDC = false; // Not used

void setPipeIndexes(void) {
  out_to_log = 0;
}

int parseArgs(int argc, const char **argv) {
  if (argc < 2) {
    printf("Usage: Load <priority> <pipe_cound(ignored)> <pipe_out(optional)>\n");
    return -1;
  }
  setPipeIndexes();
  // TODO: error checking
  priority = atoi(argv[1]);
  pipe_count = 1;
  if (argc < 4) { // Must request fds
    pid_t pid = getpid();
    connectRecvFDS(pid, pipes, pipe_count, "Load", &pinned_cpu, &priority);
  } else {
    deserializePipe(argv[3], &pipes[0]);
  }

  return 0;
}

void perCyclePrime(void) {
  struct rusage usage_stats;
  struct timeval prev_utime;

  int prime_count = 0;
  int n = 5000; // calculate primes up to n;

  // Get rusage
  getrusage(RUSAGE_SELF, &usage_stats);
  prev_utime.tv_sec = usage_stats.ru_utime.tv_sec;
  prev_utime.tv_usec = usage_stats.ru_utime.tv_usec;
  
  // do calculations
  int i, j;
  for (i = 3; i < n; i++) {
    bool prime = true;
    for (j = 2; j <= (i / 2); j++) { // Should be sqrt
      if ((i / (double)j) == (i / j)) {
        prime = false;
      }
    }
    if (prime) {
      prime_count++;
      // printf("Prime: %d\n", i);
    }
  }

  // Get rusage / output it
  getrusage(RUSAGE_SELF, &usage_stats);
  printf("Load found %d primes in: %ld - %ld\n", prime_count, usage_stats.ru_utime.tv_sec - prev_utime.tv_sec, usage_stats.ru_utime.tv_usec - prev_utime.tv_usec);
}

// Copied from icfdetect.c from libccv
static unsigned int get_current_time(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Modified from icfdetect.c from libccv
void perCyclePedestrian(void) {
  int i;
  ccv_enable_default_cache();
  ccv_dense_matrix_t* image = 0;
  ccv_icf_classifier_cascade_t* cascade = ccv_icf_read_classifier_cascade("ccv_related/pedestrian.icf");
  ccv_read("ccv_related/street.png", &image, CCV_IO_ANY_FILE | CCV_IO_RGB_COLOR);
  if (image != 0) {
    unsigned int elapsed_time = get_current_time();
    ccv_array_t* seq = ccv_icf_detect_objects(image, &cascade, 1, ccv_icf_default_params);
    elapsed_time = get_current_time() - elapsed_time;
    for (i = 0; i < seq->rnum; i++) {
      ccv_comp_t* comp = (ccv_comp_t*)ccv_array_get(seq, i);
      // This would be intersting output for checking for errors / voting
      //printf("%d %d %d %d %f\n", comp->rect.x, comp->rect.y, comp->rect.width, comp->rect.height, comp->classification.confidence);
    }
    // This should be a message to the logger.
    struct comm_msg_buffer msg_buff;
    msg_buff.length = asprintf(&(msg_buff.message), "Ped total: %d in time %dms\n", seq->rnum, elapsed_time);
    if (msg_buff.length <= 0) {
      perror("Load component failed to create message buffer");
    }
    commSendMsgBuffer(&(pipes[out_to_log]), &msg_buff);
    free(msg_buff.message);
    ccv_array_free(seq);
    ccv_matrix_free(image);
  }
  ccv_icf_classifier_cascade_free(cascade);
  ccv_disable_cache();
  
  exit(0);
}

void enterLoop(void) {
  while(1) {
    perCyclePedestrian();
  }
}

int main(int argc, const char **argv) {
  if (parseArgs(argc, argv) < 0) {
    puts("ERROR: failure parsing args.");
    return -1;
  }

  if (initController() < 0) {
    puts("ERROR: failure in setup function.");
    return -1;
  }

  enterLoop();

  return 0;
}
