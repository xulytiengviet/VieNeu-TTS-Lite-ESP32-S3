#include "wav_pcm.h"
#include <string.h>

static uint16_t le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int tag(const uint8_t *p, const char *text) {
    return memcmp(p, text, 4) == 0;
}

const char *wav_pcm_parse(const uint8_t *wav, size_t length, wav_pcm_view_t *out) {
    if (!wav || !out) return "NULL argument";
    memset(out, 0, sizeof(*out));
    if (length < 12 || !tag(wav, "RIFF") || !tag(wav + 8, "WAVE"))
        return "not RIFF/WAVE";
    uint32_t declared = le32(wav + 4);
    if (declared < 4 || (uint64_t)declared + 8U > length)
        return "RIFF truncated or invalid length";
    const size_t end = (size_t)declared + 8U;
    size_t offset = 12;
    int have_fmt = 0, have_data = 0;
    while (offset + 8U <= end) {
        const uint8_t *chunk = wav + offset;
        const uint32_t size = le32(chunk + 4);
        offset += 8;
        if ((size_t)size > end - offset)
            return "chunk beyond RIFF boundary";
        if (tag(chunk, "fmt ")) {
            if (size < 16) return "invalid fmt chunk";
            if (le16(wav + offset) != 1) return "only PCM audio supported";
            out->channels = le16(wav + offset + 2);
            out->sample_rate = le32(wav + offset + 4);
            out->bits_per_sample = le16(wav + offset + 14);
            if (out->channels != 1 || out->sample_rate != 24000 ||
                out->bits_per_sample != 16 || le16(wav + offset + 12) != 2 ||
                le32(wav + offset + 8) != 48000)
                return "expected PCM mono 16-bit 24000 Hz";
            have_fmt = 1;
        } else if (tag(chunk, "data")) {
            out->pcm = wav + offset;
            out->pcm_bytes = size;
            have_data = 1;
        }
        /* RIFF chunks are word aligned; do not wrap size calculations. */
        size_t step = (size_t)size + (size & 1U);
        if (step > end - offset) return "missing RIFF pad byte";
        offset += step;
    }
    if (!have_fmt) return "missing fmt chunk";
    if (!have_data) return "missing data chunk";
    if (!out->pcm_bytes || out->pcm_bytes % 2 != 0)
        return "empty or odd PCM payload";
    return NULL;
}
