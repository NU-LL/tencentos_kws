/*
 * wav.c
 *
 *  Created on: 2022年3月11日
 *      Author: spaceman
 */
#include "wav.h"
#include "wavhead.h"
#include "shell.h"
#include "log.h"
#include "fsl_debug_console.h"
#include "fsl_common.h"
#include "fsl_adc.h"
#include "fsl_cache.h"

#include "nnom.h"
#include "kws_weights_40epoch_cnn.h"
//#include "kws_weights_bak.h"
// enable MFCC arm FFT acceleration
//#define PLATFORM_ARM
#include "mfcc.h"


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




k_event_t audio_evt;
k_mutex_t mfcc_buf_mutex;

//防止重复打开文件
uint8_t open_file_flag = 0;

// NNoM model
nnom_model_t *model;

// 10 labels-1
//const char label_name[][10] =  {"yes", "no", "up", "down", "left", "right", "on", "off", "stop", "go", "unknow"};

// 10 labels-2
//const char label_name[][10] =  {"marvin", "sheila", "yes", "no", "left", "right", "forward", "backward", "stop", "go", "unknow"};

// full 34 labels
const char label_name[][10] =  {"backward", "bed", "bird", "cat", "dog", "down", "eight","five", "follow", "forward",
                      "four", "go", "happy", "house", "learn", "left", "marvin", "nine", "no", "off", "on", "one", "right",
                      "seven", "sheila", "six", "stop", "three", "tree", "two", "up", "visual", "yes", "zero", "unknow"};

// configuration
#define SAMP_FREQ 16000
#define AUDIO_FRAME_LEN (512) //31.25ms * 16000hz = 512, // FFT (windows size must be 2 power n)

mfcc_t * mfcc;
int16_t dma_audio_buffer[AUDIO_FRAME_LEN*2];
int16_t audio_buffer_16bit[(int)(AUDIO_FRAME_LEN*1.5)]; // an easy method for 50% overlapping


//the mfcc feature for kws
#define MFCC_LEN			(63)
#define MFCC_COEFFS_FIRST	(1)		// ignore the mfcc feature before this number
#define MFCC_COEFFS_LEN 	(13)    // the total coefficient to calculate
#define MFCC_TOTAL_NUM_BANK	(26)    // total number of filter bands
#define MFCC_COEFFS    	    (MFCC_COEFFS_LEN-MFCC_COEFFS_FIRST)

#define MFCC_FEAT_SIZE 	(MFCC_LEN * MFCC_COEFFS)

volatile float mfcc_features_f[MFCC_COEFFS];	 			// output of mfcc
volatile int8_t mfcc_features[MFCC_LEN][MFCC_COEFFS];	 // ring buffer
volatile int8_t mfcc_features_seq[MFCC_LEN][MFCC_COEFFS]; // sequencial buffer for neural network input.
uint32_t mfcc_feat_index = 0;

// debugging controls
bool is_print_abs_mean = false; // to print the mean of absolute value of the mfcc_features_seq[][]
bool is_print_mfcc  = false;    // to print the raw mfcc features at each update


uint8_t use_file_flag = 0;//kws_model_thread 线程中是否从文件中读取数据


static int32_t abs_mean(int8_t *p, size_t size)
{
	int64_t sum = 0;
	for(size_t i = 0; i<size; i++)
	{
		if(p[i] < 0)
			sum+=-p[i];
		else
			sum += p[i];
	}
	return sum/size;
}

static void quantize_data(float*din, int8_t *dout, uint32_t size, uint32_t int_bit)
{
	#define _MAX(x, y) (((x) > (y)) ? (x) : (y))
	#define _MIN(x, y) (((x) < (y)) ? (x) : (y))
	float limit = (1 << int_bit);
	for(uint32_t i=0; i<size; i++)
		dout[i] = (int8_t)(_MAX(_MIN(din[i], limit), -limit) / limit * 127);
}




//ADC中断
/* ADC1_IRQn interrupt handler */
void adc1_handler(void)
{
#define SaturaLH(N, L, H) (((N)<(L))?(L):(((N)>(H))?(H):(N)))
  /*  Place your code here */
	static uint16_t idx = 0;
	int32_t val = 0;

	//12bit: 0 ~ 4096
	//center:1.25v ==> 1552
	val = ADC_GetChannelConversionValue(ADC1, 0);//0~4096  0~1552~3104
	//0~1552~3104 ==> -32767~0~32767
	val = 21.1128*(double)val - 32767;//-32767~0~32767
	val = SaturaLH(val, -32768, 32767);

	dma_audio_buffer[idx++] = val;
	if(idx == AUDIO_FRAME_LEN)
	{
		tos_event_post_keep(&audio_evt, 1);
//		L1CACHE_CleanDCacheByRange(&dma_audio_buffer[AUDIO_FRAME_LEN], AUDIO_FRAME_LEN*sizeof(int16_t));
//		PRINTF("1\r\n");
	}

	if(idx >= AUDIO_FRAME_LEN*2)
	{
		tos_event_post_keep(&audio_evt, 2);
//		L1CACHE_CleanDCacheByRange(dma_audio_buffer, AUDIO_FRAME_LEN*sizeof(int16_t));
//		PRINTF("2\r\n");
		idx = 0;
	}
}



void kws_model_thread(void *arg)
{
	uint32_t evt;
	int16_t *p_raw_audio;
	uint32_t time;
	struct wav_header wav = {0};
	UINT br;
	FRESULT res;
	uint32_t label;
	k_time_t last_time = 0;
	float prob;
	uint32_t audio_sample_i = 0;
	int16_t F = 512;

	FIL *fp = (FIL *)arg;


	if(use_file_flag)
	{
		wavheader_read(&wav, fp);
//		wavheader_print(&wav);

		PRINTF("Information:\r\n");
		PRINTF("samplerate %d\r\n", wav.fmt_sample_rate);
		PRINTF("channels %d\r\n", wav.fmt_channels);
		PRINTF("sample bits width %d\r\n", wav.fmt_bit_per_sample);
	}

	//清空 buff
	memset(mfcc_features_f, 0, sizeof(float)*MFCC_COEFFS);
	memset(mfcc_features, 0, sizeof(int8_t)*MFCC_FEAT_SIZE);
	memset(mfcc_features_seq, 0, sizeof(int8_t)*MFCC_FEAT_SIZE);

	// calculate 13 coefficient, use number #2~13 coefficient. discard #1
	// features, offset, bands, 512fft, 0 preempha, attached_energy_to_band0
	mfcc = mfcc_create(MFCC_COEFFS_LEN, MFCC_COEFFS_FIRST, MFCC_TOTAL_NUM_BANK, AUDIO_FRAME_LEN, 0.97f, true);

	while(1)
	{
		if(use_file_flag == 0)
		{
			// wait for event and check which buffer is filled
			tos_event_pend(&audio_evt, 1|2|4, &evt, TOS_TIME_FOREVER, TOS_OPT_EVENT_PEND_ANY | TOS_OPT_EVENT_PEND_CLR);

			if(evt & 4)
				break;
			if(evt & 1)
				p_raw_audio = dma_audio_buffer;
			else if(evt & 2)
				p_raw_audio = &dma_audio_buffer[AUDIO_FRAME_LEN];

		}else
		{
			res = f_read(fp, dma_audio_buffer, AUDIO_FRAME_LEN*sizeof(int16_t), &br);
			if (res != FR_OK)
			{
				PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
				f_close(fp);
				return ;
			}
			if (br == 0)
				break;
			p_raw_audio = dma_audio_buffer;
		}




		// memory move
		// audio buffer = | 256 byte old data |   256 byte new data 1 | 256 byte new data 2 |
		//                         ^------------------------------------------|
		memcpy(audio_buffer_16bit, &audio_buffer_16bit[AUDIO_FRAME_LEN], (AUDIO_FRAME_LEN/2)*sizeof(int16_t));

		// convert it to 16 bit.
		// volume*4
		for(int i = 0; i < AUDIO_FRAME_LEN; i++)
		{
			audio_buffer_16bit[AUDIO_FRAME_LEN/2+i] = p_raw_audio[i];
		}

		for(int i=0; i<2; i++)
		{
			mfcc_compute(mfcc, &audio_buffer_16bit[i*AUDIO_FRAME_LEN/2], mfcc_features_f);

			// quantise them using the same scale as training data (in keras), by 2^n.
			quantize_data(mfcc_features_f, mfcc_features[mfcc_feat_index], MFCC_COEFFS, 3);

			// debug only, to print mfcc data on console
			if(is_print_mfcc)
			{
				for(int i=0; i<MFCC_COEFFS; i++)
					printf("%d ",  mfcc_features[mfcc_feat_index][i]);
				printf("\n");
			}

			mfcc_feat_index++;
			if(mfcc_feat_index >= MFCC_LEN)
				mfcc_feat_index = 0;
		}


		uint32_t len_first = MFCC_FEAT_SIZE - mfcc_feat_index * MFCC_COEFFS;
		uint32_t len_second = mfcc_feat_index * MFCC_COEFFS;
		memcpy(&mfcc_features_seq[0][0], &mfcc_features[0][0] + len_second,  len_first);
		memcpy(&mfcc_features_seq[0][0] + len_first, &mfcc_features[0][0], len_second);


		// debug only, to print the abs mean of mfcc output. use to adjust the dec bit (shifting)
		// of the mfcc computing.
		if(is_print_abs_mean)
			PRINTF("abs mean:%d\r\n", abs_mean((int8_t*)mfcc_features_seq, MFCC_FEAT_SIZE));

		// ML
		memcpy(nnom_input_data, mfcc_features_seq, MFCC_FEAT_SIZE);
		nnom_predict(model, &label, &prob);

		// output
		if(prob > 0.5f)
		{
			PRINTF("time : %d ms \t",tos_tick2millisec(tos_systick_get()) - last_time);
			PRINTF("%s : %d%%\r\n", (char*)&label_name[label], (int)(prob * 100));
		}
		last_time = tos_tick2millisec(tos_systick_get());
	}
	if(use_file_flag)
		f_close(fp);

	mfcc_delete(mfcc);

	PRINTF("Model inference ends\r\n");
}

void kws_init()
{
	k_err_t err;
	err = tos_event_create(&audio_evt, 0x00);
	if(err != K_ERR_NONE)
		PRINTF("create event fail");

	err = tos_mutex_create(&mfcc_buf_mutex);
	if(err != K_ERR_NONE)
		PRINTF("create mutex fail");

	// create and compile the model
	model = nnom_model_create();


	// dummy run
	model_run(model);
}

//通过读取文件系统中的音频来推理模型
static int kws(int argc, char *argv[])
{
	//当前读取的文件
	static FIL fp;

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
        	k_task_t *kws_serv_task = NULL;
        	use_file_flag = 1;
        	tos_task_create_dyn(&kws_serv_task, "kws_serv", kws_model_thread, &fp, TOS_CFG_TASK_PRIO_MAX/2, 1024, 50);
        }
        //打开的文件会在对应线程中自动关闭
    }
    else
    {
        PRINTF("%s\r\n", "command error. Usage: kws xxx.wav");
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, kws, kws, keyword spotting. Usage: kws xxx.wav);




//实时采集麦克风数据 记录到文件系统中
static int record(int argc, char *argv[])
{
	k_time_t lasttime = 0;
	k_time_t targettime = 0;
	FIL fp;
	UINT br;
	FRESULT res;
	uint32_t evt;
	struct wav_header wav = {0};
	int16_t *p_raw_audio;
	uint32_t total_length = 0;//总共写入文件数据长度
	adc_channel_config_t adcChannelConfigStruct;

    if (argc >= 3)
    {
    	targettime = atoi(argv[2]);
    	if(targettime == 0)
    	{
			PRINTF("time need > 0\r\n");
			return -1;
		}

        res = f_open(&fp, argv[1], FA_CREATE_ALWAYS	 | FA_WRITE);
        if (res != FR_OK)
        {
            PRINTF("Failure:%s\r\n", RESAULT_TO_STR(res));
            return -1;
        }
        else
        {
        	/* Configure the user channel and interrupt. */
			adcChannelConfigStruct.channelNumber                        = 4;
			adcChannelConfigStruct.enableInterruptOnConversionCompleted = true;
        	ADC_SetChannelConfig(ADC1, 0, &adcChannelConfigStruct);

        	PRINTF("start record file: %s\r\n", argv[1]);


        	//先填充一个临时头部进去
        	f_write(&fp, &wav, 44, &br);

        	//记录开始事件
        	lasttime = tos_tick2millisec(tos_systick_get());

        	while(1)
        	{
        		// 等待数据采集完毕
        		tos_event_pend(&audio_evt, 1|2, &evt, TOS_TIME_FOREVER, TOS_OPT_EVENT_PEND_ANY | TOS_OPT_EVENT_PEND_CLR);
        		if(evt & 1)
        		{
        			//填充完毕buff1
        			p_raw_audio = dma_audio_buffer;
        		}else if(evt & 2)
        		{
        			//填充完毕buff2
        			p_raw_audio = &dma_audio_buffer[AUDIO_FRAME_LEN];
        		}

        		//写入文件中
        		f_write(&fp, p_raw_audio, AUDIO_FRAME_LEN*sizeof(int16_t), &br);
        		total_length += AUDIO_FRAME_LEN*sizeof(int16_t);

        		//是否到时间
        		if(tos_tick2millisec(tos_systick_get()) - lasttime >= targettime)
        		{
        			//修改wav文件头
        			wavheader_init(&wav, 16000, 1, total_length);
        			f_rewind(&fp);
					wavheader_write(&wav, &fp);
        			break;
        		}
        	}
        	f_close(&fp);
        	PRINTF("record file: %s end\r\n", argv[1]);
        }
		//关闭adc
		adcChannelConfigStruct.enableInterruptOnConversionCompleted = false;
		ADC_SetChannelConfig(ADC1, 0, &adcChannelConfigStruct);
    }
    else
    {
        PRINTF("%s\r\n", "command error. Usage: record [filename] [ms]");
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, record, record, Recording via built-in ADC);


//实时采集麦克风输入模型
static int rt_kws(int argc, char *argv[])
{
	k_time_t lasttime = 0;
	k_time_t targettime = 0;
	adc_channel_config_t adcChannelConfigStruct;
	k_task_t *kws_serv_task = NULL;

    if (argc >= 2)
    {
    	targettime = atoi(argv[1]);
    	if(targettime == 0)
		{
			PRINTF("time need > 0\r\n");
			return -1;
		}

    	//设置
    	use_file_flag = 0;
		tos_task_create_dyn(&kws_serv_task, "kws_serv", kws_model_thread, NULL, TOS_CFG_TASK_PRIO_MAX/2, 1024, 50);

		PRINTF("start real time keyword spotting\r\n");

		//记录开始事件
		lasttime = tos_tick2millisec(tos_systick_get());

    	/* Configure the user channel and interrupt. */
		adcChannelConfigStruct.channelNumber                        = 4;
		adcChannelConfigStruct.enableInterruptOnConversionCompleted = true;
		ADC_SetChannelConfig(ADC1, 0, &adcChannelConfigStruct);

		//是否到时间
		while(tos_tick2millisec(tos_systick_get()) - lasttime < targettime)
			tos_sleep_ms(10);

		//关闭adc
		adcChannelConfigStruct.enableInterruptOnConversionCompleted = false;
		ADC_SetChannelConfig(ADC1, 0, &adcChannelConfigStruct);
		//停止线程
		tos_event_post_keep(&audio_evt, 4);

		PRINTF("end real time keyword spotting\r\n");
    }
    else
    {
        PRINTF("%s\r\n", "command error. Usage: rt_kws [ms]");
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, rt_kws, rt_kws, Real time keyword spotting. Usage: rt_kws [ms]);



