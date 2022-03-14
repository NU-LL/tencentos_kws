/*
 * Copyright (c) 2018-2020, Jianjia Ma
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-03-29     Jianjia Ma   first implementation
 */

#include "tos_k.h"
//#include "tos_shell.h"
#include "stdio.h"

#include "shell.h"
#include "log.h"

#include "nnom.h"
#include "image.h"
//#include "weights.h"
//#include "kws_weights_40epoch_cnn.h"
#include "wav.h"

//static nnom_model_t *model = NULL;

int nn_main(void)
{
	tos_sleep_ms(10);

	kws_init();

//	// create and compile the model
//	model = nnom_model_create();
//
//	// dummy run
//	model_run(model);
}


void  __attribute__((weak)) application_entry(void *arg)
{
	nn_main();
}

//// ASCII lib from (https://www.jianshu.com/p/1f58a0ebf5d9)
//const char codeLib[] = "@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'.   ";
//void print_img(int8_t * buf)
//{
//    for(int y = 0; y < 28; y++)
//	{
//        for (int x = 0; x < 28; x++)
//		{
//            int index =  69 / 127.0 * (127 - buf[y*28+x]);
//			if(index > 69) index =69;
//			if(index < 0) index = 0;
//            printf("%c",codeLib[index]);
//			printf("%c",codeLib[index]);
//        }
//        printf("\r\n");
//    }
//}

//// Do simple test using image in "image.h" with model created previously.
//void mnist(int argc, char** argv)
//{
//	uint32_t tick, time;
//	uint32_t predic_label;
//	float prob;
//	int32_t index = atoi(argv[1]);
//
//	if(index >= TOTAL_IMAGE || argc != 2)
//	{
//		printf("Please input image number within %d\r\n", TOTAL_IMAGE-1);
//		return;
//	}
//
//	printf("\r\nprediction start.. \r\n");
//	tick = tos_systick_get();
//
//	// copy data and do prediction
//	memcpy(nnom_input_data, (int8_t*)&img[index][0], 784);
//	nnom_predict(model, &predic_label, &prob);
//	time = tos_systick_get() - tick;
//
//	//print original image to console
//	print_img((int8_t*)&img[index][0]);
//
//	printf("Time: %d tick\r\n", time);
//	printf("Truth label: %d\r\n", label[index]);
//	printf("Predicted label: %d\r\n", predic_label);
//	printf("Probability: %d%%\r\n", (int)(prob*100));
//}
//SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), mnist, mnist, mnist);
//
//void nn_stat()
//{
//	model_stat(model);
//	printf("Total Memory cost (Network and NNoM): %d\r\n", nnom_mem_stat());
//}
//SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), nn_stat, nn_stat, print nn model);


