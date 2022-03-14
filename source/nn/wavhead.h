/*
 * wav.h
 *
 *  Created on: 2022年3月11日
 *      Author: spaceman
 */

#ifndef NN_WAV_HEAD_H_
#define NN_WAV_HEAD_H_

#include "tos_k.h"
#include "ff.h"


struct wav_header
{
    char  riff_id[4];                       /* "RIFF" */
    int   riff_datasize;                    /* RIFF chunk data size,exclude riff_id[4] and riff_datasize,total - 8 */

    char  riff_type[4];                     /* "WAVE" */

    char  fmt_id[4];                        /* "fmt " */
    int   fmt_datasize;                     /* fmt chunk data size,16 for pcm */
    short fmt_compression_code;             /* 1 for PCM */
    short fmt_channels;                     /* 1(mono) or 2(stereo) */
    int   fmt_sample_rate;                  /* samples per second */
    int   fmt_avg_bytes_per_sec;            /* sample_rate * channels * bit_per_sample / 8 */
    short fmt_block_align;                  /* number bytes per sample, bit_per_sample * channels / 8 */
    short fmt_bit_per_sample;               /* bits of each sample(8,16,32). */

    char  data_id[4];                       /* "data" */
    int   data_datasize;                    /* data chunk size,pcm_size - 44 */
};

int wavheader_init(struct wav_header *header, int sample_rate, int channels, int datasize);
int wavheader_read(struct wav_header *header, FIL *fp);
int wavheader_write(struct wav_header *header, FIL *fp);
void wavheader_print(struct wav_header *header);



#endif /* NN_WAV_HEAD_H_ */
