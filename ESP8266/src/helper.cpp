#include "helper.h"

inline int32_t TimeDifference(uint32_t prev, uint32_t next)
{
  return ((int32_t) (next - prev));
}

int32_t TimePassedSince(uint32_t timestamp)
{
  // Compute the number of milliSeconds passed since timestamp given.
  // Note: value can be negative if the timestamp has not yet been reached.
  return TimeDifference(timestamp, millis());
}

bool TimeReached(uint32_t timer)
{
  // Check if a certain timeout has been reached.
  const int32_t passed = TimePassedSince(timer);
  return (passed >= 0);
}

void SetNextTimeInterval(uint32_t& timer, const uint32_t step)
{
  timer += step;
  const int32_t passed = TimePassedSince(timer);
  if (passed < 0) { return; }   // Event has not yet happened, which is fine.
  if (static_cast<unsigned long>(passed) > step) {
    // No need to keep running behind, start again.
    timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  timer = millis() + (step - passed);
}

int32_t TimePassedSinceUsec(uint32_t timestamp)
{
  return TimeDifference(timestamp, micros());
}

bool TimeReachedUsec(uint32_t timer)
{
  // Check if a certain timeout has been reached.
  const int32_t passed = TimePassedSinceUsec(timer);
  return (passed >= 0);
}