/*
 * wavhead.c
 *
 *  Created on: 2022年3月12日
 *      Author: spaceman
 */
#include "wavhead.h"
#include "shell.h"
#include "log.h"
#include "fsl_debug_console.h"


#define RESAULT_TO_STR(r) resault_to_str_map[r]
static char *resault_to_str_map[] =
    {
        "FR_OK",                  /* (0) Succeeded */
        "FR_DISK_ERR",            /* (1) A hard error occurred in the low level disk I/O layer */
        "FR_INT_ERR",             /* (2) Assertion failed */
        "FR_NOT_READY",           /* (3) The physical drive cannot work */
        "FR_NO_FILE",             /* (4) Could not find the file */
        "FR_NO_PATH",             /* (5) Could not find the path */
        "FR_INVALID_NAME",        /* (6) The path name format is invalid */
        "FR_DENIED",              /* (7) Access denied due to prohibited access or directory full */
        "FR_EXIST",               /* (8) Access denied due to prohibited access */
        "FR_INVALID_OBJECT",      /* (9) The file/directory object is invalid */
        "FR_WRITE_PROTECTED",     /* (10) The physical drive is write protected */
        "FR_INVALID_DRIVE",       /* (11) The logical drive number is invalid */
        "FR_NOT_ENABLED",         /* (12) The volume has no work area */
        "FR_NO_FILESYSTEM",       /* (13) There is no valid FAT volume */
        "FR_MKFS_ABORTED",        /* (14) The f_mkfs() aborted due to any problem */
        "FR_TIMEOUT",             /* (15) Could not get a grant to access the volume within defined period */
        "FR_LOCKED",              /* (16) The operation is rejected according to the file sharing policy */
        "FR_NOT_ENOUGH_CORE",     /* (17) LFN working buffer could not be allocated */
        "FR_TOO_MANY_OPEN_FILES", /* (18) Number of open files > FF_FS_LOCK */
        "FR_INVALID_PARAMETER"    /* (19) Given parameter is invalid */
};


/* read and write integer from file stream */
static int get_int(FIL *fp)
{
	UINT br;
    char *s;
    int i;
    s = (char *)&i;
    size_t len = sizeof(int);
    int n = 0;
    for (; n < len; n++)
    {
    	f_read(fp, &s[n], 1, &br);
        // s[n] = getc(fp);
    }
    return i;
}

static int put_int(int i, FIL *fp)
{
	UINT br;
    char *s;
    s = (char *)&i;
    size_t len = sizeof(int);
    int n = 0;
    for (; n < len; n++)
    {
    	f_write(fp, &s[n], 1, &br);
//        f_putc(s[n], fp);
        // putc(s[n], fp);
    }

    return i;
}

static short int get_sint(FIL *fp)
{
	UINT br;
    char *s;
    short int i;
    s = (char *)&i;
    size_t len = sizeof(short);
    int n = 0;
    for (; n < len; n++)
    {
    	f_read(fp, &s[n], 1, &br);
        // s[n] = getc(fp);
    }

    return i;
}

static short int put_sint(short int i, FIL *fp)
{
	UINT br;
    char *s;
    s = (char *)&i;
    size_t len = sizeof(short);
    int n = 0;
    for (; n < len; n++)
    {
    	f_write(fp, &s[n], 1, &br);
//        f_putc(s[n], fp);
        // putc(s[n], fp);
    };

    return i;
}

int wavheader_init(struct wav_header *header, int sample_rate, int channels, int datasize)
{
    if (header == NULL)
        return -1;

    memcpy(header->riff_id, "RIFF", 4);
    header->riff_datasize = datasize + 44 - 8;

    memcpy(header->riff_type, "WAVE", 4);

    memcpy(header->fmt_id, "fmt ", 4);
    header->fmt_datasize = 16;
    header->fmt_compression_code = 1;
    header->fmt_channels = channels;
    header->fmt_sample_rate = sample_rate;
    header->fmt_bit_per_sample = 16;
    header->fmt_avg_bytes_per_sec = header->fmt_sample_rate * header->fmt_channels * header->fmt_bit_per_sample / 8;
    header->fmt_block_align = header->fmt_bit_per_sample * header->fmt_channels / 8;

    memcpy(header->data_id, "data", 4);
    header->data_datasize = datasize;

    return 0;
}

int wavheader_read(struct wav_header *header, FIL *fp)
{
    FRESULT res;
    UINT br;
    if (fp == NULL)
        return -1;

    res = f_read(fp, header->riff_id, 4, &br);
    if (res != FR_OK)
        goto error;
    header->riff_datasize = get_int(fp);
    res = f_read(fp, header->riff_type, 4, &br);
    if (res != FR_OK)
        goto error;
    res = f_read(fp, header->fmt_id, 4, &br);
    if (res != FR_OK)
        goto error;
    header->fmt_datasize = get_int(fp);
    header->fmt_compression_code = get_sint(fp);
    header->fmt_channels = get_sint(fp);
    header->fmt_sample_rate = get_int(fp);
    header->fmt_avg_bytes_per_sec = get_int(fp);
    header->fmt_block_align = get_sint(fp);
    header->fmt_bit_per_sample = get_sint(fp);
    res = f_read(fp, header->data_id, 4, &br);
    if (res != FR_OK)
        goto error;
    header->data_datasize = get_int(fp);

    return 0;
error:
    PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
    f_close(fp);
    return -1;
}

int wavheader_write(struct wav_header *header, FIL *fp)
{
    FRESULT res;
    UINT br;
    if (fp == NULL)
        return -1;

    res = f_write(fp, header->riff_id, 4, &br);
    if (res != FR_OK)
        goto error;
    put_int(header->riff_datasize, fp);
    res = f_write(fp, header->riff_type, 4, &br);
    if (res != FR_OK)
        goto error;
    res = f_write(fp, header->fmt_id, 4, &br);
    if (res != FR_OK)
        goto error;
    put_int(header->fmt_datasize, fp);
    put_sint(header->fmt_compression_code, fp);
    put_sint(header->fmt_channels, fp);
    put_int(header->fmt_sample_rate, fp);
    put_int(header->fmt_avg_bytes_per_sec, fp);
    put_sint(header->fmt_block_align, fp);
    put_sint(header->fmt_bit_per_sample, fp);
    res = f_write(fp, header->data_id, 4, &br);
    if (res != FR_OK)
        goto error;
    put_int(header->data_datasize, fp);

    return 0;
error:
    PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
    f_close(fp);
    return -1;
}

void wavheader_print(struct wav_header *header)
{
    PRINTF("header.riff_id: %c%c%c%c\r\n", header->riff_id[0], header->riff_id[1], header->riff_id[2], header->riff_id[3]);
    PRINTF("header.riff_datasize: %d\r\n", header->riff_datasize);
    PRINTF("header.riff_type: %c%c%c%c\r\n", header->riff_type[0], header->riff_type[1], header->riff_type[2], header->riff_type[3]);
    PRINTF("header.fmt_id: %c%c%c%c\r\n", header->fmt_id[0], header->fmt_id[1], header->fmt_id[2], header->fmt_id[3]);
    PRINTF("header.fmt_datasize: %d\r\n", header->fmt_datasize);
    PRINTF("header.fmt_compression_code: %hd\r\n", header->fmt_compression_code);
    PRINTF("header.fmt_channels: %hd\r\n", header->fmt_channels);
    PRINTF("header.fmt_sample_rate: %d\r\n", header->fmt_sample_rate);
    PRINTF("header.fmt_avg_bytes_per_sec: %d\r\n", header->fmt_avg_bytes_per_sec);
    PRINTF("header.fmt_block_align: %hd\r\n", header->fmt_block_align);
    PRINTF("header.fmt_bit_per_sample: %hd\r\n", header->fmt_bit_per_sample);
    PRINTF("header.data_id: %c%c%c%c\r\n", header->data_id[0], header->data_id[1], header->data_id[2], header->data_id[3]);
    PRINTF("header.data_datasize: %d\r\n", header->data_datasize);
}


#define WAV_BUFFER_SIZE     (32)


static int readwav(int argc, char *argv[])
{
    FIL fp;
    struct wav_header wav = {0};

    if (argc >= 2)
    {
        FRESULT res;
        res = f_open(&fp, argv[1], FA_READ);
        if (res != FR_OK)
        {
            PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
            return -1;
        }
        else
        {
        	PRINTF("start read file:%s\r\n", argv[1]);
            wavheader_read(&wav, &fp);
            wavheader_print(&wav);

            PRINTF("Information:\r\n");
            PRINTF("samplerate %d\r\n", wav.fmt_sample_rate);
            PRINTF("channels %d\r\n", wav.fmt_channels);
            PRINTF("sample bits width %d\r\n", wav.fmt_bit_per_sample);


            char buf[WAV_BUFFER_SIZE];
            UINT br;
            while (1)
            {
                res = f_read(&fp, buf, WAV_BUFFER_SIZE, &br);
                if (res != FR_OK)
                {
                    PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
                    f_close(&fp);
                    return -1;
                }
                if (br == 0)
                {
                    break;
                }
                logHexDumpAll(buf, WAV_BUFFER_SIZE);
                break;
                // for(UINT i=0; i<br; i++)
                // {
                //     PRINTF("%c", buf[i]);
                // }
            }
        }
    }
    else
    {
        PRINTF("%s\r\n", "command error. Usage: read path");
        return -1;
    }
    f_close(&fp);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, readwav, readwav, Read FILE. Usage: readwav xxx.wav);

//SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), readwav, readwav, Read FILE. Usage: readwav xxx.wav);

