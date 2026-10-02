#ifndef __SHOW_H__
#define __SHOW_H__

extern void show_cmd_line(void);
extern int  get_usr_cmd(char *cmd, int max_len);
extern int  parse_usr_cmd(char *cmd, char **cmdarg, int max_arg);

#endif
