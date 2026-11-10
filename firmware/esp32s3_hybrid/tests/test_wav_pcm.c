#include "wav_pcm.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void le16(uint8_t *p, uint16_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static void le32(uint8_t *p, uint32_t n) {
    p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
    p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24);
}
static size_t make_wav(uint8_t *b) {
    memset(b, 0, 96);
    memcpy(b, "RIFF", 4);
    le32(b + 4, 44);
    memcpy(b + 8, "WAVEfmt ", 8);
    le32(b + 16, 16);
    le16(b + 20, 1);
    le16(b + 22, 1);
    le32(b + 24, 24000);
    le32(b + 28, 48000);
    le16(b + 32, 2);
    le16(b + 34, 16);
    memcpy(b + 36, "data", 4);
    le32(b + 40, 8);
    for (int i = 0; i < 8; ++i) b[44 + i] = (uint8_t)(i * 10);
    return 52;
}
int main(void) {
    uint8_t b[96];
    wav_pcm_view_t result;
    size_t n = make_wav(b);
    assert(wav_pcm_parse(b, n, &result) == NULL);
    assert(result.pcm == b + 44 && result.pcm_bytes == 8);
    assert(result.sample_rate == 24000 && result.channels == 1);
    assert(wav_pcm_parse(b, n - 1, &result) != NULL);
    b[0] = 'X';
    assert(wav_pcm_parse(b, n, &result) != NULL);
    make_wav(b);
    le16(b + 22, 2);
    assert(wav_pcm_parse(b, n, &result) != NULL);
    make_wav(b);
    le32(b + 24, 16000);
    assert(wav_pcm_parse(b, n, &result) != NULL);
    make_wav(b);
    le16(b + 20, 3);
    assert(wav_pcm_parse(b, n, &result) != NULL);
    make_wav(b);
    le32(b + 40, 9999);
    assert(wav_pcm_parse(b, n, &result) != NULL);
    make_wav(b);
    /* Insert unknown odd-sized chunk (JUNK, 3 bytes + 1 byte pad) before data. */
    memmove(b + 48, b + 36, 16);
    memcpy(b + 36, "JUNK", 4);
    le32(b + 40, 3);
    b[44] = 1; b[45] = 2; b[46] = 3; b[47] = 0;
    le32(b + 4, 56);
    assert(wav_pcm_parse(b, 64, &result) == NULL);
    assert(result.pcm == b + 56 && result.pcm_bytes == 8);
    make_wav(b);
    memcpy(b + 36, "JUNK", 4);
    assert(wav_pcm_parse(b, n, &result) != NULL);
    assert(wav_pcm_parse(NULL, 0, &result) != NULL);
    assert(wav_pcm_parse(b, n, NULL) != NULL);
    puts("PASS: WAV PCM parser valid, extra-chunk, unsupported, corrupt, missing, truncated");
    return 0;
}
