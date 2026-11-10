/* Small, ESP-independent WAV PCM parser for host and MCU tests.
 * Apache-2.0, VieNeu Edge Lab contributors.
 */
#ifndef VIENEU_WAV_PCM_H
#define VIENEU_WAV_PCM_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *pcm;
    size_t pcm_bytes;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
} wav_pcm_view_t;

/* Returns NULL on success, otherwise an explanatory static error string.
 * Only RIFF/WAVE PCM mono, 16-bit, 24000 Hz is supported for this demo.
 * Data refers to the original byte array (caller keeps buffer allocated).
 */
const char *wav_pcm_parse(const uint8_t *wav, size_t length, wav_pcm_view_t *out);

#endif
