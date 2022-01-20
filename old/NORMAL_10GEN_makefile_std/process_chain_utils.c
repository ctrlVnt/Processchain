#include <stdio.h>
#include "process_chain_utils.h"

void red()
{
  printf("\033[1;31m");
}

void yellow()
{
  printf("\033[0;33m");
}

void reset()
{
  printf("\033[0m");
}

void cyan()
{
  printf("\033[0;36m");
}

void green()
{
  printf("\033[0;32m");
}

void purple()
{
  printf("\033[1;35m");
}

void blue()
{
  printf("\033[1;34m");
}