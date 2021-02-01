#pragma once

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))
#define MAXPATH 2000
/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);
/* Built-in command struct and lookup table */
struct build_in_function {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} ;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_cd(struct tokens *tokens);
int cmd_pwd(struct tokens *tokens);
int cmd_kill(struct tokens *tokens);

int lookup(char cmd[]);
int exec_build_in_cmd(int function_index, struct tokens *tokens);