#ifndef SPEAKER_H
#define SPEAKER_H

#include <stdint.h>

void speaker_on(uint32_t freq_hz);
void speaker_off(void);
void speaker_beep(uint32_t freq_hz, uint32_t ms);
void speaker_jingle(void);

#endif
