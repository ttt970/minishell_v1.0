#ifndef __EXECCMD_H__
#define __EXECCMD_H__

/* 主循环运行标志，cmd_exit 置 0 以退出 shell */
extern int g_running;

extern void do_usr_cmd(int cmdcnt, char *cmdarg[]);

#endif
