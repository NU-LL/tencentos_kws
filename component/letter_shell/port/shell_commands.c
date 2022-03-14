/*----------------------------------------------------------------------------
 * Tencent is pleased to support the open source community by making TencentOS
 * available.
 *
 * Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
 * If you have downloaded a copy of the TencentOS binary from Tencent, please
 * note that the TencentOS binary is licensed under the BSD 3-Clause License.
 *
 * If you have downloaded a copy of the TencentOS source code from Tencent,
 * please note that TencentOS source code is licensed under the BSD 3-Clause
 * License, except for the third-party components listed below which are
 * subject to different license terms. Your integration of TencentOS into your
 * own projects may require compliance with the BSD 3-Clause License, as well
 * as the other licenses applicable to the third-party components included
 * within TencentOS.
 *---------------------------------------------------------------------------*/

#include "shell.h"


//void printf(char *fmt, ...)
//{
//	Shell* shell = shellGetCurrent();
//	SHELL_LOCK(shell);
//	shellPrint(shell, fmt, ...);
//	SHELL_UNLOCK(shell);
//}


static long version(void)
{
	printf("\r\nWelcome to TencentOS tiny(%s)\r\n", TOS_VERSION);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, version, version, show TencentOS tiny version information);



static void task_shell_walker(k_task_t *task)
{
    char *state_str = "ABNORMAL";
    
    if (!task) {
        return;
    }

    state_str = state_str;
//    printf("task name: %s", task->name);

    if (tos_task_curr_task_get() == task) {
        state_str = "RUNNING";
    } else if (task->state == K_TASK_STATE_PENDTIMEOUT_SUSPENDED) {
        state_str = "PENDTIMEOUT_SUSPENDED";
    } else if (task->state == K_TASK_STATE_PEND_SUSPENDED) {
        state_str = "PEND_SUSPENDED";
    } else if (task->state == K_TASK_STATE_SLEEP_SUSPENDED) {
        state_str = "SLEEP_SUSPENDED";
    } else if (task->state == K_TASK_STATE_PENDTIMEOUT) {
        state_str = "PENDTIMEOUT";
    } else if (task->state == K_TASK_STATE_SUSPENDED) {
        state_str = "SUSPENDED";
    } else if (task->state == K_TASK_STATE_PEND) {
        state_str = "PEND";
    } else if (task->state == K_TASK_STATE_SLEEP) {
        state_str = "SLEEP";
    } else if (task->state == K_TASK_STATE_READY) {
        state_str = "READY";
    }
    
//    printf("task stat: %s", state_str);
//    printf("stk size : %d", task->stk_size);
//    printf("stk base : 0x%p", task->stk_base);
//    printf("stk top  : 0x%p", task->stk_base + task->stk_size);
    
    int depth = -1;
#if TOS_CFG_TASK_STACK_DRAUGHT_DEPTH_DETACT_EN > 0u
//    int depth;
    
    if (tos_task_stack_draught_depth(task, &depth) != K_ERR_NONE) {
        depth = -1;
    }
    
//    printf("stk depth: %d", depth);
#endif /* TOS_CFG_TASK_STACK_DRAUGHT_DEPTH_DETACT_EN */
    
    printf("%-*s %3d %21s ", K_TASK_NAME_LEN_MAX, task->name, task->prio, state_str);
    printf("0x%08x 0x%08x    %02d%%   0x%08x", task->stk_base, task->stk_size, (depth==-1)?-1:(depth * 100 / task->stk_size), task->tick_expires);

    printf("\r\n");
}

__STATIC__ int cmd_ps(int argc, char *argv[])
{
	printf("%-*s %3s %21s ", K_TASK_NAME_LEN_MAX, "thread name", "pri", "status");
	printf("%10s %10s %8s %10s\r\n", "stack base", "stack size", "max used", "left tick");
    tos_task_walkthru(task_shell_walker);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, ps, cmd_ps, show task info);



__STATIC__ int cmd_free(int argc, char *argv[])
{
#if TOS_CFG_MMHEAP_EN > 0u
    k_mmheap_info_t pool_info;
    uint32_t total;

    if (tos_mmheap_check(&pool_info) != K_ERR_NONE) {
        printf("mmheap check fail!\r\n");
        return -1;
    }

    total = pool_info.used + pool_info.free;
    printf("      %10s    %10s    %10s\r\n", "total", "used", "free");
    printf("Pool: %10d    %10d    %10d\r\n", total, pool_info.used, pool_info.free);
    
    return 0;
#else
    printf("TOS_CFG_MMHEAP_EN is disenable!\r\n");
    return -1;
#endif /* TOS_CFG_MMHEAP_EN */
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, free, cmd_free, show mmheap info);









