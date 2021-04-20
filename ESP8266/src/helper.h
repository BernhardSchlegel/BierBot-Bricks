#include <Arduino.h>

int32_t TimeDifference(uint32_t prev, uint32_t next);
int32_t TimePassedSince(uint32_t timestamp);
bool TimeReached(uint32_t timer);
void SetNextTimeInterval(uint32_t& timer, const uint32_t step);
int32_t TimePassedSinceUsec(uint32_t timestamp);
bool TimeReachedUsec(uint32_t timer);