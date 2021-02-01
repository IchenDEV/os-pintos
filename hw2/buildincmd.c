#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tokenizer.h"
#include "buildincmd.h"

struct build_in_function cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_cd, "cd", "change dir of this shell"},
    {cmd_pwd, "pwd", "The pwd utility writes the absolute pathname of the current working directory to the standard output"},
    {cmd_kill, "kill", "kill"},
};

int exec_build_in_cmd(int function_index, struct tokens *tokens)
{
  return cmd_table[function_index].fun(tokens);
}
/* Looks up the built-in command, if it exists. */
int lookup(char cmd[])
{
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(struct build_in_function); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens)
{
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(struct build_in_function); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens)
{
  exit(0);
}
/* change dir of this shell */
int cmd_cd(struct tokens *tokens)
{
  char *path = tokens_get_token(tokens, 1);
  if (path == NULL)
    return -1;
  chdir(path);
  cmd_pwd(NULL);
  return 0;
}

int cmd_pwd(unused struct tokens *tokens)
{
  char buffer[MAXPATH];
  getcwd(buffer, MAXPATH);
  printf("%s\n", buffer);
  return 0;
}

/* change dir of this shell */
int cmd_kill(struct tokens *tokens)
{
  char *Sig = tokens_get_token(tokens, 1);
  if (Sig == NULL)
    return -1;
  Sig++;
  int signum = 0;
  if (strcmp(Sig, "TERM") == 0)
  {
    signum = SIGTERM;
  }
  else if (strcmp(Sig, "INT") == 0)
  {
    signum = SIGINT;
  }
  else if (strcmp(Sig, "QUIT") == 0)
  {
    signum = SIGQUIT;
  }
  else if (strcmp(Sig, "KILL") == 0)
  {
    signum = SIGKILL;
  }
  else if (strcmp(Sig, "TSTP") == 0)
  {
    signum = SIGTSTP;
  }
  else if (strcmp(Sig, "CONT") == 0)
  {
    signum = SIGCONT;
  }
  else if (strcmp(Sig, "TTIN") == 0)
  {
    signum = SIGTTIN;
  }
  else if (strcmp(Sig, "TTOU") == 0)
  {
    signum = SIGTTOU;
  }
  char *pid = tokens_get_token(tokens, 2);

  kill(SIGINT, atoi(pid));
  return 0;
}