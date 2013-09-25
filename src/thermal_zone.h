#ifndef _THERMAL_ZONE_H_
#define _THERMAL_ZONE_H_

struct thermal_zone;

struct thermal_zone *thermal_zone_open(const char *dir);
void thermal_zone_close(struct thermal_zone *tz);

void thermal_zone_enable(struct thermal_zone *tz);
void thermal_zone_disable(struct thermal_zone *tz);

int thermal_zone_read(struct thermal_zone *tz, char *buf, unsigned int blen);
int thermal_zone_set_trip(struct thermal_zone *tz, int lower, int upper);

#endif
