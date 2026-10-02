#include "show.h"
#include "head.h"

void show_cmd_line(void)
{
	char path[512] = {0};

	if (getcwd(path, sizeof(path)) == NULL)
	{
		strcpy(path, "?");
	}
	printf("\033[1;35mlinux@ubuntu\033[0m:\033[1;32m%s\033[0m$ ", path);
	fflush(stdout);//为防止放入缓冲区，所以刷新
}

/* 读一行用户输入。返回 0 成功，-1 表示 EOF(Ctrl-D)。 */
int get_usr_cmd(char *cmd, int max_len)
{
	if (fgets(cmd, max_len, stdin) == NULL)
	{
		return -1;
	}

	/* 只裁掉行尾的换行符本身，空行/超长行都安全 */
	cmd[strcspn(cmd, "\n")] = '\0';
	return 0;
}

/* 把 cmd 按空白切词写进 cmdarg[]，结尾补 NULL。返回词数。 */
int parse_usr_cmd(char *cmd, char **cmdarg, int max_arg)
{
	int i = 0;
	char *tok;

	tok = strtok(cmd, " ");
	while (tok != NULL && i < max_arg - 1)
	{
		cmdarg[i++] = tok;
		tok = strtok(NULL, " ");
	}
	cmdarg[i] = NULL;

	return i;
}
