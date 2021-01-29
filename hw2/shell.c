#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "tokenizer.h"
#include "buildincmd.h"
#include "shell.h"

char *find_real_path(struct tokens *tokens)
{
  char *pathenv = getenv("PATH");
  char *pathvar = malloc(strlen(pathenv) * sizeof(char));
  strcpy(pathvar, pathenv);
  char *save_ptr = NULL;
  char *current_path = strtok_r(pathvar, ":", &save_ptr);
  char *file = tokens_get_token(tokens, 0);
  char *real_full_path = NULL;

  bool file_exist = access(file, F_OK) == 0;
  if (file_exist)
  {
    real_full_path = malloc(strlen(file) * sizeof(char));
    strcpy(real_full_path, file);
  }
  while (file != NULL && !file_exist && current_path != NULL)
  {
    char *full_path = malloc((strlen(current_path) + strlen(file) + 8) * sizeof(char));
    strcpy(full_path, current_path);
    strcat(full_path, "/");
    strcat(full_path, file);
    bool file_exist = access(full_path, F_OK) == 0;
    if (file_exist)
    {
      real_full_path = malloc(strlen(full_path) * sizeof(char));
      strcpy(real_full_path, full_path);
      break;
    }
    current_path = strtok_r(NULL, ":", &save_ptr);
    free(full_path);
  }
  if (pathvar)
    free(pathvar);

  return real_full_path;
}

int exec_file(struct tokens *tokens)
{
  int token_len = tokens_get_length(tokens);
  char *real_full_path = find_real_path(tokens);
  if (real_full_path == NULL)
    return -2;

  bool is_background = strcmp(tokens_get_token(tokens, token_len - 1), "&") == 0;

  int argc = is_background ? token_len : (token_len + 1);

  char **args = malloc((argc) * sizeof(char *));

  for (int i = 0; i < argc - 1; i++)
    args[i] = tokens_get_token(tokens, i);
  args[argc - 1] = NULL;

  pid = vfork();
  if (pid == 0)
  {
    if (!is_background)
    {
      setpgid(getpid(), getpid());
      tcsetpgrp(shell_terminal, getpid());
    }
    execv(real_full_path, args);
    exit(-1);
  }

  int status = 0;
  if (!is_background)
  {
    wait(&status);
    tcsetpgrp(shell_terminal, shell_pgid);
    pid = 0;
    if (args)
      free(args);
  }
  return status;
}
void sighandler(int signum)
{
  if (pid > 0)
  {
    kill(pid, signum);
  }
  else
  {
    tcsetpgrp(shell_terminal, getpid());
  }
}

/* Initialization procedures for this shell */
void init_shell()
{
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;
  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive)
  {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();
    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  signal(SIGINT, sighandler);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
}

int main(unused int argc, unused char *argv[])
{
  init_shell();

  static char line[4096];
  static int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin))
  {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);
    char *exec_cmd = tokens_get_token(tokens, 0);
    struct tokens *pipe_token;
    bool has_pipe_instream = false;
    do
    {
      if (has_pipe_instream)
        freopen("/tmp/shellpipe", "r+", stdin);
      else
        freopen("/dev/tty", "r+", stdin);

      pipe_token = tokens_split(tokens, "|");

      if (pipe_token != NULL)
      {
        freopen("/tmp/shellpipe", "w+", stdout);
        has_pipe_instream = true;
      }
      else
      {
        freopen("/dev/tty", "w+", stdout);
        has_pipe_instream = false;
      }

      struct tokens *file_tokens = tokens_split(tokens, ">");
      if (file_tokens != NULL)
      {
        char *outfile = tokens_get_token(file_tokens, 0);
        freopen(outfile, "w+", stdout);
      }

      file_tokens = tokens_split(tokens, "<");
      if (file_tokens != NULL)
      {
        char *infile = tokens_get_token(file_tokens, 0);
        freopen(infile, "r+", stdin);
      }

      if (exec_cmd != NULL)
      {
        /* Find which built-in function to run. */
        int function_index = lookup(exec_cmd);

        int state = 0;
        if (function_index >= 0)
          state = exec_build_in_cmd(function_index, tokens);
        else
          state = exec_file(tokens);

        switch (state)
        {
        case -2:
        {
          printf("[%d] %s not exist.\n", state, exec_cmd);
          break;
        }
        case 2:
        {
          printf("[%d] %s Killed.\n", state, exec_cmd);
          break;
        }
        case 65280:
        {
          printf("[%d]This shell doesn't know how to run %s\n", state, exec_cmd);
        }
        }
      }

      tokens = pipe_token;
      if (!file_tokens)
        tokens_destroy(file_tokens);

    } while (tokens != NULL);

    /* Clean up memory */
    tokens_destroy(tokens);
    /* Clean up memory */

    freopen("/dev/tty", "w+", stdout);
    freopen("/dev/tty", "r+", stdin);
    init_shell();
    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);
  }
  return 0;
}
