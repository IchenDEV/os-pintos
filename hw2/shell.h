#pragma once

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;


void init_shell();
pid_t pid=0;

enum exit_code{
  not_find=-1,
  ok=0,
};


