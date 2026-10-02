#include "head.h"
#include "show.h"
#include "execcmd.h"

int g_running = 1;

int main(void)
{
	char cmd[512] = {0};
	char *cmdarg[64] = {NULL};
	int cmdcnt = 0;

	while (g_running)
	{
		show_cmd_line();

		if (get_usr_cmd(cmd, sizeof(cmd)) < 0)
		{
			break;              /* EOF / Ctrl-D 退出 */
		}

		cmdcnt = parse_usr_cmd(cmd, cmdarg, 64); //cmdcnt表示解析出来的部分有多少个
		if (cmdcnt == 0)
		{
			continue;           /* 空行(即输了空格)，直接下一轮 */
		}

		do_usr_cmd(cmdcnt, cmdarg);
	}

	return 0;
}
