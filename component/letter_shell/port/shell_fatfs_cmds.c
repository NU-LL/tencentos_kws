/*
 * shell_fatfs_cmds.c
 *
 *  Created on: 2022年2月9日
 *      Author: spaceman
 */


/*
 * @Author: your name
 * @Date: 2021-10-09 17:14:58
 * @LastEditTime: 2021-10-11 21:26:22
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: /xcmd/extensions/fs_cmds/fs_cmds.c
 */

#include "shell.h"
#include "ff.h"

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
static char g_prompt[128];
static FIL fp;
static FILINFO fno;

#define HELP_DF ("Show Disk Info. Usage: df")
#define HELP_LS ("List information about the FILEs. Usage: ls")
#define HELP_CD ("Change the shell working directory. Usage: cd path")
#define HELP_READ ("Read FILE. Usage: read path")
#define HELP_RM ("Delete FILE. Usage: rm [-r:dir] path")
#define HELP_MV ("Move FILE. Usage: mv path")
#define HELP_SYNC ("Sync Usage: sync")
#define HELP_MKDIR ("Make DIR. Usage: mkdir path")
#define HELP_TOUCH ("Create empty FILE. Usage: touch path")
#define HELP_WRITE ("Write str to FILE. Usage: Write [-a:append] path str")

static FRESULT scan_files(
    char *path /* Start node to be scanned (***also used as work area***) */
)
{
    FRESULT res;
    DIR dir;
    res = f_stat(path, &fno);
    if ((res != FR_OK) || fno.fattrib & AM_DIR)
    {
        res = f_opendir(&dir, path); /* Open the directory */
        if (res == FR_OK)
        {
            for (;;)
            {
                res = f_readdir(&dir, &fno); /* Read a directory item */
                if (res != FR_OK || fno.fname[0] == 0)
                    break; /* Break on error or end of dir */
                if (fno.fattrib & AM_DIR)
                { /* It is a directory */
                    printf("\x1B[34m"
                            "%s"
                            "\x1B[0m"
                            "  ",
                            fno.fname);
                }
                else
                { /* It is a file. */
                    printf("%s  ", fno.fname);
                }
            }
            f_closedir(&dir);
        }
    }
    else
    {
        printf("%s  ", fno.fname);
    }
    return res;
}

static FRESULT rm_dir(char *path)
{
    FRESULT res;
    DIR dir;
    res = f_stat(path, &fno);
    if (res != FR_OK)
    {
        return res;
    }
    if (fno.fattrib & AM_DIR)
    {
        res = f_opendir(&dir, path); /* Open the directory */
        if (res == FR_OK)
        {
            while (1)
            {
                res = f_readdir(&dir, &fno); /* Read a directory item */
                if (res != FR_OK || fno.fname[0] == 0)
                    break;                /* Break on error or end of dir */
                if (fno.fattrib & AM_DIR) /* It is a directory */
                {
                    UINT i = strlen(path);
                    sprintf(&path[i], "/%s", fno.fname);
                    res = rm_dir(path); /* Enter the directory */
                    if (res != FR_OK)
                        break;
                    path[i] = 0;
                    f_unlink(fno.fname);
                }
                else
                {
                    UINT i = strlen(path);
                    sprintf(&path[i], "/%s", fno.fname);
                    f_unlink(path);
                    path[i] = 0;
                }
            }
            f_closedir(&dir);
        }
        f_unlink(path);
    }
    else
    {
        f_unlink(path);
    }
    return res;
}

static FRESULT df(char* path, DWORD* totle_byte, DWORD* free_byte)
{
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;
    FRESULT res;
    /* Get volume information and free clusters of drive 1 */
    res = f_getfree(path, &fre_clust, &fs);
    if (res == FR_OK)
    {
        /* Get total sectors and free sectors */
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;
        #if FF_MAX_SS != FF_MIN_SS
            *totle_byte = tot_sect * fs->ssize;
            *free_byte = fre_sect * fs->ssize;
        #endif
            *totle_byte = tot_sect * FF_MAX_SS;
            *free_byte = fre_sect * FF_MAX_SS;
    }
    return res;
}

static int cmd_df(int argc, char* argv[])
{
    FRESULT res;
    DWORD fre_bytes, tot_bytes;
    res = df("0:", &tot_bytes, &fre_bytes);
    if (res != FR_OK)
    {
        printf("0: Failure:%s\r\n", RESAULT_TO_STR(res));
        return -1;
    }
    printf("0:/ %lu/%lu KiB.\r\n", fre_bytes/1024, tot_bytes / 1024);

    res = df("1:", &tot_bytes, &fre_bytes);
    if (res != FR_OK)
    {
        printf("1: Failure:%s\r\n", RESAULT_TO_STR(res));
        return -1;
    }
    printf("1:/ %lu/%lu KiB.\r\n", fre_bytes/1024, tot_bytes / 1024);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, df, cmd_df, HELP_DF);

static int cmd_ls(int argc, char *argv[])
{
    if (argc == 1)
    {
        scan_files("./");
    }
    else
    {
        scan_files(argv[1]);
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, ls, cmd_ls, HELP_LS);

static int cmd_cd(int argc, char *argv[])
{
    if (argc >= 2)
    {
        FRESULT res;
        f_chdrive(argv[1]);
        res = f_chdir(argv[1]);
        if (res != FR_OK)
        {
            printf("Failure:%s\r\n", RESAULT_TO_STR(res));
        }
        else
        {
            res = f_getcwd(g_prompt, 128);
            shellSetPath(shellGetCurrent(), g_prompt);
//            strcat(g_prompt, "> ");
//            xcmd_set_prompt(g_prompt);
        }
    }
    else
    {
        printf("%s\r\n", HELP_CD);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, cd, cmd_cd, HELP_CD);

static int cmd_read(int argc, char *argv[])
{
    if (argc >= 2)
    {
        FRESULT res;
        res = f_open(&fp, argv[1], FA_READ);
        if (res != FR_OK)
        {
            printf("Failure:%s\r\n", RESAULT_TO_STR(res));
            return -1;
        }
        else
        {
            char buf[128];
            UINT br;
            while (1)
            {
                res = f_read(&fp, buf, 128, &br);
                if (res != FR_OK)
                {
                    printf("Failure:%s\r\n", RESAULT_TO_STR(res));
                    f_close(&fp);
                    return -1;
                }
                if (br == 0)
                {
                    break;
                }
                for(UINT i=0; i<br; i++)
                {
                    printf("%c", buf[i]);
                }
            }
        }
    }
    else
    {
        printf("%s\r\n", HELP_READ);
        return -1;
    }
    f_close(&fp);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, read, cmd_read, HELP_READ);

static int cmd_rm(int argc, char *argv[])
{
    FRESULT res;
    uint8_t dir_flag = 0;
    uint8_t param_num = 2;
    /* 查找可选参数 */
    if (strcmp(argv[1], "-r") == 0)
    {
        dir_flag = 1;
        param_num = 3;
    }

    if (argc >= param_num)
    {
        if (dir_flag)
        {
            res = rm_dir(argv[2]);
            if (res != FR_OK)
            {
                printf("Failure:%s\r\n", RESAULT_TO_STR(res));
                return -1;
            }
        }
        else
        {
            res = f_stat(argv[1], &fno);
            if (res != FR_OK)
            {
                printf("Failure:%s\r\n", RESAULT_TO_STR(res));
                return -1;
            }
            if (fno.fattrib & AM_DIR)
            {
                printf("Failure:%s\r\n", "PATH is DIR");
            }
            else
            {
                res = f_unlink(argv[1]);
                if (res != FR_OK)
                {
                    printf("Failure:%s\r\n", RESAULT_TO_STR(res));
                    return -1;
                }
            }
        }
    }
    else
    {
        printf("%s\r\n", HELP_RM);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, rm, cmd_rm, HELP_RM);

static int cmd_mv(int argc, char *argv[])
{
    if (argc >= 3)
    {
        FRESULT res;
        res = f_rename(argv[1], argv[2]);
        if (res != FR_OK)
        {
            printf("Failure:%s\r\n", RESAULT_TO_STR(res));
            return -1;
        }
    }
    else
    {
        printf("%s\r\n", HELP_MV);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, mv, cmd_mv, HELP_MV);

static int cmd_sync(int argc, char *argv[])
{
//     f_sync(&fp);
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, sync, cmd_sync, HELP_SYNC);

static int cmd_mkdir(int argc, char *argv[])
{
    if (argc >= 2)
    {
        FRESULT res;
        res = f_mkdir(argv[1]);
        if (res != FR_OK)
        {
            printf("Failure:%s\r\n", RESAULT_TO_STR(res));
            return 0;
        }
    }
    else
    {
        printf("%s\r\n", HELP_MKDIR);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, mkdir, cmd_mkdir, HELP_MKDIR);

static int cmd_touch(int argc, char *argv[])
{
    if (argc >= 2)
    {
        FRESULT res;
        res = f_open(&fp, argv[1], FA_CREATE_NEW);
        if ((res != FR_OK) && (res != FR_EXIST))
        {
            printf("Failure:%s\r\n", RESAULT_TO_STR(res));
            return -1;
        }
        f_close(&fp);
    }
    else
    {
        printf("%s\r\n", HELP_TOUCH);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, touch, cmd_touch, HELP_TOUCH);

static int cmd_write(int argc, char *argv[])
{
    uint8_t append_flag = 0;
    uint8_t param_num = 3;
    /* 查找可选参数 */
    if (strcmp(argv[1], "-a") == 0)
    {
        append_flag = 1;
        param_num = 4;
    }

    FRESULT res = FR_INVALID_PARAMETER;
    char *str = NULL;
    if (argc >= param_num)
    {
        if (append_flag)
        {
            res = f_open(&fp, argv[2], FA_WRITE | FA_OPEN_APPEND);
            str = argv[3];
        }
        else
        {
            res = f_open(&fp, argv[1], FA_WRITE | FA_CREATE_ALWAYS);
            str = argv[2];
        }

        if (res != FR_OK)
        {
            printf("Failure:%s", RESAULT_TO_STR(res));
            return -1;
        }
        else
        {
            UINT br;
            res = f_write(&fp, str, strlen(str), &br);
            res = f_write(&fp, "\n", 1, &br);
            f_close(&fp);
            if (res != FR_OK)
            {
                printf("Failure:%s", RESAULT_TO_STR(res));
                return -1;
            }
        }
    }
    else
    {
        printf("%s\r\n", HELP_WRITE);
        return -1;
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN)|SHELL_CMD_DISABLE_RETURN, write, cmd_write, HELP_WRITE);

