/* Frame-sized entry point for SceAudiodec. Keep the original superframe API
 * used by NGS unchanged. Byte consumption includes each block's alignment. */
int port_atrac9_frame(atrac9_decoder *d, const uint8_t *in, uint32_t bytes,
                     uint32_t first, float *out, uint32_t *consumed) {
    bits b = {in, 0, bytes * 8u};
    uint32_t channel = 0;
    int good = 1;
    *consumed = 0;
    for (uint32_t i = 0; good && i < d->blocks; i++) {
        good = decode_block(d, &d->block[i], &b, first != 0,
                            out + channel, d->info.channels);
        channel += d->block[i].channels;
        b.at = (b.at + 7u) & ~7u;
    }
    if (good && b.at <= b.end) { *consumed = b.at / 8u; return 1; }
    memset(out, 0, (size_t)d->info.frame_samples * d->info.channels * sizeof(float));
    atrac9_reset(d);
    return 0;
}
