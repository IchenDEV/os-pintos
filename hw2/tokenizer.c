#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "tokenizer.h"

static void *vector_push(char ***pointer, size_t *size, void *elem)
{
  *pointer = (char **)realloc(*pointer, sizeof(char *) * (*size + 1));
  (*pointer)[*size] = elem;
  *size += 1;
  return elem;
}

static void *copy_word(char *source, size_t n)
{
  source[n] = '\0';
  char *word = (char *)malloc(n + 1);
  strncpy(word, source, n + 1);
  return word;
}

struct tokens *tokenize(const char *line)
{
  if (line == NULL)
  {
    return NULL;
  }

  static char token[4096];
  size_t n = 0, n_max = 4096;
  struct tokens *tokens;
  size_t line_length = strlen(line);

  tokens = (struct tokens *)malloc(sizeof(struct tokens));
  tokens->tokens_length = 0;
  tokens->tokens = NULL;
  tokens->buffers_length = 0;
  tokens->buffers = NULL;

  const int MODE_NORMAL = 0,
            MODE_SQUOTE = 1,
            MODE_DQUOTE = 2;
  int mode = MODE_NORMAL;

  for (unsigned int i = 0; i < line_length; i++)
  {
    char c = line[i];
    if (mode == MODE_NORMAL)
    {
      if (c == '\'')
      {
        mode = MODE_SQUOTE;
      }
      else if (c == '"')
      {
        mode = MODE_DQUOTE;
      }
      else if (c == '\\')
      {
        if (i + 1 < line_length)
        {
          token[n++] = line[++i];
        }
      }
      else if (isspace(c))
      {
        if (n > 0)
        {
          void *word = copy_word(token, n);
          vector_push(&tokens->tokens, &tokens->tokens_length, word);
          n = 0;
        }
      }
      else
      {
        token[n++] = c;
      }
    }
    else if (mode == MODE_SQUOTE)
    {
      if (c == '\'')
      {
        mode = MODE_NORMAL;
      }
      else if (c == '\\')
      {
        if (i + 1 < line_length)
        {
          token[n++] = line[++i];
        }
      }
      else
      {
        token[n++] = c;
      }
    }
    else if (mode == MODE_DQUOTE)
    {
      if (c == '"')
      {
        mode = MODE_NORMAL;
      }
      else if (c == '\\')
      {
        if (i + 1 < line_length)
        {
          token[n++] = line[++i];
        }
      }
      else
      {
        token[n++] = c;
      }
    }
    if (n + 1 >= n_max)
      abort();
  }

  if (n > 0)
  {
    void *word = copy_word(token, n);
    vector_push(&tokens->tokens, &tokens->tokens_length, word);
    n = 0;
  }
  return tokens;
}

size_t tokens_get_length(struct tokens *tokens)
{
  if (tokens == NULL)
  {
    return 0;
  }
  else
  {
    return tokens->tokens_length;
  }
}

char *tokens_get_token(struct tokens *tokens, size_t n)
{
  if (tokens == NULL || n >= tokens->tokens_length)
  {
    return NULL;
  }
  else
  {
    return tokens->tokens[n];
  }
}

struct tokens *tokens_split(struct tokens *tokens, char *det)
{
  if (tokens == NULL || det == NULL)
    return NULL;
  int split_pos = -1;
  for (int i = 0; i < tokens->tokens_length; i++)
  {
    char *temp_token = tokens_get_token(tokens, i);
    if (strcmp(temp_token, det) == 0)
    {
      split_pos = i;

      break;
    }
  }
  if (split_pos == -1)
    return NULL;
  int new_len = tokens->tokens_length - split_pos - 1;
  if (new_len <= 0)
  {
    return NULL;
  }
  struct tokens *new_token = malloc(sizeof(struct tokens));
  new_token->tokens = &tokens->tokens[split_pos + 1];
  new_token->tokens_length = new_len;
  tokens->tokens_length = split_pos;
  return new_token;
}

void tokens_destroy(struct tokens *tokens)
{
  if (tokens == NULL)
  {
    return;
  }
  for (int i = 0; i < tokens->tokens_length; i++)
  {
    free(tokens->tokens[i]);
  }
  for (int i = 0; i < tokens->buffers_length; i++)
  {
    free(tokens->buffers[i]);
  }
  if (tokens->tokens)
  {
    free(tokens->tokens);
  }
  free(tokens);
}
