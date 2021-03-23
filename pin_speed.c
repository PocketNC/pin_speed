#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <hal.h>

#include <pthread.h>

int pulses = 0;
pthread_mutex_t pulsesMutex;

struct gpiod_line_event event;
struct gpiod_chip *chip = NULL;
struct gpiod_line *line = NULL;

void *countPulses(void *args) {
  while(true) {
    int ret = gpiod_line_event_read(line, &event);
    if(ret == 0) {
      pthread_mutex_lock(&pulsesMutex);
      ++pulses;
      pthread_mutex_unlock(&pulsesMutex);
    }
  }
}

typedef struct {
  hal_float_t *frequency;
  hal_float_t *period;
} pin_speed_data_t;

void cleanup(struct gpiod_chip* chip, struct gpiod_line* line, int hal_comp_id) {
  if(line != NULL) {
    gpiod_line_release(line);
  }

  if(chip != NULL) {
    gpiod_chip_close(chip);
  }

  if(hal_comp_id >= 0) {
    hal_exit(hal_comp_id);
  }
}

#define MAX_CHIP_LENGTH 15

static inline void diff_time(struct timespec *a, struct timespec *b, struct timespec *out) {
  out->tv_sec = a->tv_sec - b->tv_sec;
  out->tv_nsec = a->tv_nsec - b->tv_nsec;
  if(out->tv_nsec < 0) {
    out->tv_sec -= 1;
    out->tv_nsec += 1000000000L;
  }
}

int main(int argc, char **argv) {
  int hal_comp_id; 
  char* compName;

  struct timespec ts = { 0, 100000000 };
  int chipNum = -1;
  int lineNum = -1;
  if(argc >= 4) {
    chipNum = atoi( argv[1] );
    lineNum = atoi( argv[2] );
    compName = argv[3];
  } else {
    printf("Usage: pin_speed <chip number> <line number> <component name>\n");
  }

  if(chipNum < 0 || lineNum < 0) {
    printf("Usage: pin_speed <chip number> <line number> <component name>\n");
    return -1;
  }

  char chipname[MAX_CHIP_LENGTH];
  snprintf(chipname, MAX_CHIP_LENGTH, "gpiochip%d", chipNum);

  pin_speed_data_t *haldata;

  int ret;

  chip = gpiod_chip_open_by_name(chipname);
  if(!chip) {
    printf("Failed to open chip number %d.\n", chipNum);
    return -1;
  }

  line = gpiod_chip_get_line(chip, (unsigned int)lineNum);
  if(!line) {
    printf("Failed to get line %d.\n", lineNum);
    cleanup(chip, NULL, -1);
    return -1;
  }

  ret = gpiod_line_request_rising_edge_events(line, compName);
  if(ret < 0) {
    printf("Failed to request rising edge events.\n");
    cleanup(chip, line, -1);
    return -1;
  }

  if(pthread_mutex_init(&pulsesMutex, NULL) != 0) {
    printf("Failed to create pulses mutex.\n");
    cleanup(chip, line, -1);
    return -1;
  }

  pthread_t thread_id;
  if(pthread_create(&thread_id, NULL, countPulses, NULL) != 0) {
    printf("Failed to create thread.\n");
    cleanup(chip, line, -1);
    return -1;
  }

  hal_comp_id = hal_init(compName);
  if(hal_comp_id < 0) {
    printf("%s: ERROR: hal_init failed\n", compName);
    cleanup(chip, line, -1);
    return -1;
  }

  haldata = (pin_speed_data_t*)hal_malloc(sizeof(pin_speed_data_t));
  if(haldata == 0) {
    printf("%s: ERROR: unable to allocate shared memory\n", compName);
    cleanup(chip, line, hal_comp_id);
    return -1;
  }

  ret = hal_pin_float_newf(HAL_OUT, &(haldata->frequency), hal_comp_id, "%s.frequency", compName); 
  if(ret != 0) {
    cleanup(chip, line, hal_comp_id);
    return -1;
  }
  ret = hal_pin_float_newf(HAL_OUT, &(haldata->period), hal_comp_id, "%s.period", compName); 
  if(ret != 0) {
    cleanup(chip, line, hal_comp_id);
    return -1;
  }

  *(haldata->frequency) = 0;
  *(haldata->period) = 0;

  hal_ready(hal_comp_id);

  struct timespec lastEvent;
  struct timespec now;
  struct timespec diff;
  timespec_get(&lastEvent, TIME_UTC);

  float period;
  float frequency;
  while(true) {
    nanosleep(&ts, NULL);

    timespec_get(&now, TIME_UTC);
    diff_time(&now, &lastEvent, &diff);

    int pulsesSinceLastCheck;
    pthread_mutex_lock(&pulsesMutex);
    pulsesSinceLastCheck = pulses;
    pulses = 0;
    pthread_mutex_unlock(&pulsesMutex);

    if(pulsesSinceLastCheck > 0) {
      period = ( (float)diff.tv_sec + (float)diff.tv_nsec/1000000000L ) / pulsesSinceLastCheck;
      frequency = 1./period;
    } else {
      period = 0;
      frequency = 0;
    }

    *(haldata->frequency) = frequency;
    *(haldata->period) = period;
    lastEvent = now;
  }
}
