#include <stdint.h>
#include "wave_gen.h"

void freq_source_add_note(struct source *s, uint32_t timestamp, int note, uint32_t duration);
struct source *freq_source_create(int pin);
