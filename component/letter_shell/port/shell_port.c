/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2019-02-22
 * 
 * @copyright (c) 2019 Letter
 * 
 */
#include "tos_k.h"
#include "tos_hal.h"

#include "stdio.h"
//#include "fsl_debug_console.h"
#include "fsl_lpuart.h"

#include "shell.h"
//#include "shell_fs.h"
#include "log.h"


#define OUTPUT_UART				LPUART1
#define FIFO_BUFFER_SIZE		50
#define SHELL_TASK_STK_SIZE		4096

Shell shell;
char shellBuffer[512];
char fifo_buffer[FIFO_BUFFER_SIZE];

static k_mutex_t shellMutex;
static k_sem_t shell_rx_sem;
static k_chr_fifo_t shell_rx_fifo;

static k_task_t shell_task;
static uint8_t shell_task_stk[SHELL_TASK_STK_SIZE];


void uartLogWrite(char *buffer, short len)
{
	LPUART_WriteBlocking(OUTPUT_UART, (uint8_t *)buffer, len);
}


/**
 * @brief 用户shell写
 * 
 * @param data 数据
 * @param len 数据长度
 * 
 * @return short 实际写入的数据长度
 */
short userShellWrite(char *data, unsigned short len)
{
#if 1
    status_t status;
    status = LPUART_WriteBlocking(OUTPUT_UART, (uint8_t *)data, len);
	if (status != kStatus_Success) {
		return 0;
	}
	return len;

#else
    /* if using c lib printf through uart, a simpler one is: */
	printf("%.*s", len, data);
	return len;
#endif
}



void shell_input_byte(uint8_t data)
{
    if (tos_chr_fifo_push(&shell_rx_fifo, data) == K_ERR_NONE) {
        tos_sem_post(&shell_rx_sem);
    }
}


int shell_getchar(void)
{
    uint8_t chr;
    k_err_t err;

    if (tos_sem_pend(&shell_rx_sem, TOS_TIME_FOREVER) != K_ERR_NONE) {
        return -1;
    }

    err = tos_chr_fifo_pop(&shell_rx_fifo, &chr);

    return err == K_ERR_NONE ? chr : -1;
}


/**
 * @brief 用户shell读
 *
 * @param data 数据
 * @param len 数据长度
 *
 * @return short 实际读取到
 */
short userShellRead(char *data, unsigned short len)
{
	unsigned short old = len;
	int ret = 0;
	for(;len != 0; len--,data++)
	{
		ret = shell_getchar();
		if(ret == -1)
			goto _exit;
		*data = ret;
	}
_exit:
	return old - len;
}

/**
 * @brief 用户shell上锁
 * 
 * @param shell shell
 * 
 * @return int 0
 */
int userShellLock(Shell *shell)
{
	tos_mutex_pend_timed(&shellMutex, TOS_TIME_FOREVER);
    return 0;
}

/**
 * @brief 用户shell解锁
 * 
 * @param shell shell
 * 
 * @return int 0
 */
int userShellUnlock(Shell *shell)
{
	tos_mutex_post(&shellMutex);
    return 0;
}







//
//size_t getcwd(char *path, size_t maxLen){
//	return 0;
//}
//
//size_t chdir(char *path){
//	return 0;
//}
//
///**
// * @brief 列出文件
// *
// * @param path 路径
// * @param buffer 结果缓冲
// * @param maxLen 最大缓冲长度
// * @return size_t 0
// */
//size_t listdir(char *path, char *buffer, size_t maxLen){
////	DIR *dir;
////	struct dirent *ptr;
////	int i;
////	dir = opendir(path);
////	memset(buffer, 0, maxLen);
////	while((ptr = readdir(dir)) != NULL)
////	{
////		strcat(buffer, ptr->d_name);
////		strcat(buffer, "\t");
////	}
////	closedir(dir);
//	return 0;
//}






static Log uartLog =
		{ .write = uartLogWrite, .active = true, .level = LOG_ALL };

//static ShellFs shellFs;
//static char shellPathBuffer[512] = "/";
/**
 * @brief 用户shell初始化
 * 
 */
void userShellInit(void)
{
	k_err_t ret = K_ERR_NONE;
    ret |= tos_mutex_create(&shellMutex);
    ret |= tos_chr_fifo_create(&shell_rx_fifo, fifo_buffer, FIFO_BUFFER_SIZE);
    ret |= tos_sem_create(&shell_rx_sem, (k_sem_cnt_t)0u);
    if(ret == K_ERR_OBJ_PTR_NULL)  // mutex为空指针
	{
    	printf("shell init error![create ipc error]");
		return ;
	}


    shell.write = userShellWrite;
    shell.read = userShellRead;
#if SHELL_USING_LOCK == 1
    shell.lock = userShellLock;
    shell.unlock = userShellUnlock;
#endif
    shellInit(&shell, shellBuffer, 512);

    // 注册log
    logRegister(&uartLog, &shell);

//    logDebug("hello world");
//    logInfo("[info] hello world");

//    // shellFs
//    shellFs.getcwd = getcwd;
//	shellFs.chdir = chdir;
//	shellFs.listdir = listdir;
//	shellFsInit(&shellFs, shellPathBuffer, 512);
//	shellSetPath(&shell, shellPathBuffer);
//	shellCompanionAdd(&shell, SHELL_COMPANION_ID_FS, &shellFs);



    //创建并启动shell task
    if (tos_task_create(&shell_task, "shell_task", shellTask, &shell, TOS_CFG_TASK_PRIO_MAX/2-1, shell_task_stk, SHELL_TASK_STK_SIZE, 0) != K_ERR_NONE)
    {
        logError("shell task creat failed");
    }
}
//CEVENT_EXPORT(EVENT_INIT_STAGE2, userShellInit);

