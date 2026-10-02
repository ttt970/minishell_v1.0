#include "execcmd.h"
#include "head.h"

/* =========================================================================
 * 工具函数
 * ========================================================================= */

/* 完整写出 len 字节：处理部分写(short write)和 EINTR 中断。成功返回 0。 */
static int write_all(int fd, const char *buf, size_t len)
{
	size_t done = 0;

	while (done < len)
	{
		ssize_t n = write(fd, buf + done, len - done);

		if (n < 0)
		{
			if (errno == EINTR)
			{
				continue;
			}
			return -1;
		}
		done += (size_t)n;
	}
	return 0;
}

/* 取路径最后一段（basename），不改动入参。 */
static const char *path_base(const char *path)
{
	const char *slash = strrchr(path, '/');

	return (slash == NULL) ? path : slash + 1;
}

/* 把 src 复制到 dst，目标权限取 mode。成功返回 0。 */
static int copy_file(const char *src, const char *dst, mode_t mode)
{
	int fin = -1, fout = -1;
	char buf[4096];
	ssize_t n;
	int ret = -1;

	fin = open(src, O_RDONLY);
	if (fin < 0)
	{
		perror(src);
		return -1;
	}

	fout = open(dst, O_WRONLY | O_CREAT | O_TRUNC, mode);
	if (fout < 0)
	{
		perror(dst);
		close(fin);
		return -1;
	}

	/* 新建文件时的 mode 会被 umask 削弱，这里显式对齐到源权限 */
	fchmod(fout, mode & 07777);

	while ((n = read(fin, buf, sizeof(buf))) > 0)
	{
		if (write_all(fout, buf, (size_t)n) < 0)
		{
			perror("write");
			goto out;
		}
	}
	if (n < 0)
	{
		perror("read");
		goto out;
	}

	ret = 0;
out:
	close(fin);
	close(fout);
	return ret;
}

/* qsort 用的字符串比较 */
static int cmp_str(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* =========================================================================
 * 各命令实现：统一签名 int cmd_xxx(int argc, char *argv[])
 * 约定：成功返回 0，失败返回 -1
 * ========================================================================= */

static int cmd_cd(int argc, char *argv[])
{
	const char *target;

	if (argc < 2)
	{
		target = getenv("HOME");
		if (target == NULL)
		{
			fprintf(stderr, "cd: HOME not set\n");
			return -1;
		}
	}
	else
	{
		target = argv[1];
	}

	if (chdir(target) != 0)
	{
		fprintf(stderr, "cd: %s: %s\n", target, strerror(errno));
		return -1;
	}
	return 0;
}

static int cmd_pwd(int argc, char *argv[])
{
	char path[1024];

	(void)argc;
	(void)argv;

	if (getcwd(path, sizeof(path)) == NULL)
	{
		perror("pwd");
		return -1;
	}
	printf("%s\n", path);
	return 0;
}

static int cmd_echo(int argc, char *argv[])
{
	int i;

	for (i = 1; i < argc; i++)
	{
		if (i > 1)
		{
			putchar(' ');
		}
		printf("%s", argv[i]); 
	}
	putchar('\n');
	return 0;
}

static int cmd_exit(int argc, char *argv[])
{
	(void)argc;	//避免出现 参数-Wunused-parameter 的警告
	(void)argv;

	g_running = 0;
	return 0;
}

static int cmd_touch(int argc, char *argv[])
{
	int i;
	int ret = 0;

	if (argc < 2)
	{
		fprintf(stderr, "touch: missing file operand\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		int fd = open(argv[i], O_WRONLY | O_CREAT, 0666);

		if (fd < 0)
		{
			fprintf(stderr, "touch: cannot touch '%s': %s\n",
					argv[i], strerror(errno));
			ret = -1;
			continue;
		}
		close(fd);

		/* 文件已存在时，必须靠 utimensat 才能真正更新时间戳 */
		if (utimensat(AT_FDCWD, argv[i], NULL, 0) != 0)
		{
			fprintf(stderr, "touch: cannot touch '%s': %s\n",
					argv[i], strerror(errno));
			ret = -1;
		}
	}
	return ret;
}

static int cmd_mkdir(int argc, char *argv[])
{
	int i;
	int ret = 0;

	if (argc < 2)
	{
		fprintf(stderr, "mkdir: missing operand\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if (mkdir(argv[i], 0777) != 0)
		{
			fprintf(stderr, "mkdir: cannot create directory '%s': %s\n",
					argv[i], strerror(errno));
			ret = -1;
		}
	}
	return ret;
}

static int cmd_rmdir(int argc, char *argv[])
{
	int i;
	int ret = 0;

	if (argc < 2)
	{
		fprintf(stderr, "rmdir: missing operand\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if (rmdir(argv[i]) != 0)
		{
			fprintf(stderr, "rmdir: failed to remove '%s': %s\n",
					argv[i], strerror(errno));
			ret = -1;
		}
	}
	return ret;
}

static int cmd_rm(int argc, char *argv[])
{
	int i;
	int ret = 0;

	if (argc < 2)
	{
		fprintf(stderr, "rm: missing operand\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		if (unlink(argv[i]) != 0)
		{
			if (errno == EISDIR)
			{
				fprintf(stderr, "rm: cannot remove '%s': Is a directory\n",
						argv[i]);
			}
			else
			{
				fprintf(stderr, "rm: cannot remove '%s': %s\n",
						argv[i], strerror(errno));
			}
			ret = -1;
		}
	}
	return ret;
}

static int cmd_cat(int argc, char *argv[])
{
	int i;
	int ret = 0;
	char buf[4096];

	if (argc < 2)
	{
		fprintf(stderr, "cat: missing file operand\n");
		return -1;
	}

	for (i = 1; i < argc; i++)
	{
		int fd;
		ssize_t n;

		fd = open(argv[i], O_RDONLY);
		if (fd < 0)
		{
			fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
			ret = -1;
			continue;
		}

		while ((n = read(fd, buf, sizeof(buf))) > 0)
		{
			if (write_all(STDOUT_FILENO, buf, (size_t)n) < 0)
			{
				perror("cat: write");
				close(fd);
				return -1;
			}
		}
		if (n < 0)
		{
			fprintf(stderr, "cat: %s: %s\n", argv[i], strerror(errno));
			ret = -1;
		}
		close(fd);
	}
	return ret;
}

static int cmd_ls(int argc, char *argv[])
{
	const char *dirpath = (argc >= 2) ? argv[1] : ".";
	DIR *dir;
	struct dirent *ent;
	char **names = NULL;
	size_t cnt = 0, cap = 0, i;

	dir = opendir(dirpath);
	if (dir == NULL)
	{
		fprintf(stderr, "ls: cannot access '%s': %s\n",
				dirpath, strerror(errno));
		return -1;
	}

	while ((ent = readdir(dir)) != NULL)
	{
		char *name;

		if (ent->d_name[0] == '.')
		{
			continue;               /* 跳过隐藏文件 */
		}

		if (cnt == cap)
		{
			size_t newcap = (cap == 0) ? 16 : cap * 2;
			char **tmp = realloc(names, newcap * sizeof(char *));

			if (tmp == NULL)
			{
				perror("ls: realloc");
				break;
			}
			names = tmp;
			cap = newcap;
		}

		/* d_name 指向的缓冲会被下次 readdir 覆盖，必须拷贝保存 */
		name = strdup(ent->d_name);
		if (name == NULL)
		{
			perror("ls: strdup");
			break;
		}
		names[cnt++] = name;
	}

	closedir(dir);

	if (cnt > 1)
	{
		qsort(names, cnt, sizeof(char *), cmp_str);
	}

	for (i = 0; i < cnt; i++)
	{
		printf("%s\n", names[i]);
		free(names[i]);
	}
	free(names);

	return 0;
}

static int cmd_cp(int argc, char *argv[])
{
	const char *src, *dst;
	struct stat st_src, st_dst;
	char target[1024];

	if (argc != 3)
	{
		fprintf(stderr, "cp: usage: cp source target\n");
		return -1;
	}
	src = argv[1];
	dst = argv[2];

	if (stat(src, &st_src) != 0)
	{
		fprintf(stderr, "cp: cannot stat '%s': %s\n", src, strerror(errno));
		return -1;
	}
	if (S_ISDIR(st_src.st_mode))
	{
		fprintf(stderr, "cp: '%s' is a directory (recursive copy not supported)\n", src);
		return -1;
	}

	if (stat(dst, &st_dst) == 0)
	{
		if (S_ISDIR(st_dst.st_mode))
		{
			/* 目标是目录：拷成“目录名/源文件名” */
			if (snprintf(target, sizeof(target), "%s/%s",
						 dst, path_base(src)) >= (int)sizeof(target))
			{
				fprintf(stderr, "cp: path too long\n");
				return -1;
			}
			dst = target;
		}

		/* 源与目标同一文件（同 dev + 同 ino）时拒绝，否则 O_TRUNC 会清空源 */
		if (stat(dst, &st_dst) == 0 &&
			st_dst.st_dev == st_src.st_dev &&
			st_dst.st_ino == st_src.st_ino)
		{
			fprintf(stderr, "cp: '%s' and '%s' are the same file\n", src, dst);
			return -1;
		}
	}

	return copy_file(src, dst, st_src.st_mode & 07777);
}

static int cmd_mv(int argc, char *argv[])
{
	const char *src, *dst;
	struct stat st_dst;
	char target[1024];

	if (argc != 3)
	{
		fprintf(stderr, "mv: usage: mv source target\n");
		return -1;
	}
	src = argv[1];
	dst = argv[2];

	if (stat(dst, &st_dst) == 0 && S_ISDIR(st_dst.st_mode))
	{
		if (snprintf(target, sizeof(target), "%s/%s",
					 dst, path_base(src)) >= (int)sizeof(target))
		{
			fprintf(stderr, "mv: path too long\n");
			return -1;
		}
		dst = target;
	}

	if (rename(src, dst) == 0)
	{
		return 0;               /* 同文件系统内：一次原子改名搞定 */
	}

	if (errno != EXDEV)
	{
		fprintf(stderr, "mv: cannot move '%s' to '%s': %s\n",
				src, dst, strerror(errno));
		return -1;
	}

	/* 跨文件系统：退化为“复制 + 删源” */
	{
		struct stat st_src;

		if (stat(src, &st_src) != 0)
		{
			fprintf(stderr, "mv: cannot stat '%s': %s\n", src, strerror(errno));
			return -1;
		}
		if (S_ISDIR(st_src.st_mode))
		{
			fprintf(stderr, "mv: cannot move directory '%s' across filesystems\n", src);
			return -1;
		}
		if (copy_file(src, dst, st_src.st_mode & 07777) != 0)
		{
			return -1;          /* 复制失败则保留源文件 */
		}
		if (unlink(src) != 0)
		{
			fprintf(stderr, "mv: cannot remove '%s': %s\n", src, strerror(errno));
			return -1;
		}
	}
	return 0;
}

/* =========================================================================
 * 命令表 + help
 * ========================================================================= */

typedef struct
{
	const char *name;
	const char *usage;
	int (*fn)(int argc, char *argv[]);//函数指针
} cmd_entry;

static int cmd_help(int argc, char *argv[]);   /* 表里要引用，先声明 */

static const cmd_entry g_cmds[] = {
	{ "cd",    "cd [dir]        切换目录（无参数回 HOME）",   cmd_cd    },
	{ "pwd",   "pwd             打印当前工作目录",            cmd_pwd   },		//——ok
	{ "ls",    "ls [dir]        列出目录内容（按字母排序）",  cmd_ls    },
	{ "cp",    "cp <src> <dst>  复制文件",                    cmd_cp    },
	{ "mv",    "mv <src> <dst>  移动/重命名文件",             cmd_mv    },
	{ "touch", "touch <file>... 创建文件 / 更新时间戳",       cmd_touch },
	{ "mkdir", "mkdir <dir>...  创建目录",                    cmd_mkdir },		//——ok
	{ "rmdir", "rmdir <dir>...  删除空目录",                  cmd_rmdir },		//——ok
	{ "rm",    "rm <file>...    删除文件",                    cmd_rm    },
	{ "cat",   "cat <file>...   显示文件内容",                cmd_cat   },		//——ok
	{ "echo",  "echo <arg>...   打印参数",                    cmd_echo  },		//——ok
	{ "help",  "help            显示本帮助",                  cmd_help  },       //——ok
	{ "exit",  "exit            退出 minishell",              cmd_exit  },		 //——ok
	{ NULL,    NULL,            NULL                                      },  /* 哨兵 */
};

static int cmd_help(int argc, char *argv[])
{
	size_t i;

	(void)argc;
	(void)argv;

	printf("minishell - 内置命令（均用 Linux 系统调用实现）\n");
	for (i = 0; g_cmds[i].name != NULL; i++)
	{
		printf("  %s\n", g_cmds[i].usage);
	}
	return 0;
}

/* =========================================================================
 * 分发
 * ========================================================================= */

void do_usr_cmd(int cmdcnt, char *cmdarg[])
{
	size_t i;

	if (cmdcnt <= 0 || cmdarg == NULL || cmdarg[0] == NULL)
	{
		return;
	}

	for (i = 0; g_cmds[i].name != NULL; i++)
	{
		if (strcmp(cmdarg[0], g_cmds[i].name) == 0)
		{
			g_cmds[i].fn(cmdcnt, cmdarg);
			return;
		}
	}

	fprintf(stderr, "minishell: %s: command not found\n", cmdarg[0]);
}
