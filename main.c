#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
    return 1;
  }

  char *p = argv[1];

  printf("  .globl main\n");
  printf("main:\n");
  printf("  ldr x0, =%ld\n", strtol(p, &p, 10));

  while (*p) {
    if (*p == '+') {
      p++;
      printf("  ldr x1, =%ld\n", strtol(p, &p, 10));
      printf("  add x0, x0, x1\n");
      continue;
    }

    if (*p == '-') {
      p++;
      printf("  ldr x1, =%ld\n", strtol(p, &p, 10));
      printf("  sub x0, x0, x1\n");
      continue;
    }

    fprintf(stderr, "unexpected character: '%c'\n", *p);
    return 1;
  }

  printf("  ret\n");
  return 0;
}
