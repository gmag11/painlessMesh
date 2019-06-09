/**
 * Wrapper file, which is used to test on PC hardware
 */
#ifndef ARDUINO_WRAP_H
#define ARDUINO_WRAP_H

#include <sys/time.h>
#include <unistd.h>

#define F(string_literal) string_literal
#define ARDUINO_ARCH_ESP8266

#ifndef NULL
#define NULL 0
#endif

inline unsigned long millis() {
  struct timeval te;
  gettimeofday(&te, NULL);  // get current time
  long long milliseconds =
      te.tv_sec * 1000LL + te.tv_usec / 1000;  // calculate milliseconds
  // printf("milliseconds: %lld\n", milliseconds);
  return milliseconds;
}

inline unsigned long micros() {
  struct timeval te;
  gettimeofday(&te, NULL);  // get current time
  long long milliseconds = te.tv_sec * 1000000LL + te.tv_usec;
  return milliseconds;
}

inline void delay(int i) { usleep(i); }

inline void yield() {}

struct IPAddress {};

#endif
