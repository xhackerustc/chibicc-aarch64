#include "chibicc.h"

static int depth;

static void push(void) {
  printf("  str x0, [sp, #-8]!\n");
  depth++;
}

static void pop(char *arg) {
  printf("  ldr %s, [sp], #8\n", arg);
  depth--;
}

static void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  ldr x0, =%d\n", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    printf("  neg x0, x0\n");
    return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("x1");

  switch (node->kind) {
  case ND_ADD:
    printf("  add x0, x0, x1\n");
    return;
  case ND_SUB:
    printf("  sub x0, x0, x1\n");
    return;
  case ND_MUL:
    printf("  mul x0, x0, x1\n");
    return;
  case ND_DIV:
    printf("  sdiv x0, x0, x1\n");
    return;
  case ND_EQ:
  case ND_NE:
  case ND_LT:
  case ND_LE:
    printf("  cmp x0, x1\n");

    if (node->kind == ND_EQ)
      printf("  cset x0, eq\n");
    else if (node->kind == ND_NE)
      printf("  cset x0, ne\n");
    else if (node->kind == ND_LT)
      printf("  cset x0, lt\n");
    else if (node->kind == ND_LE)
      printf("  cset x0, le\n");

    return;
  }

  error("invalid expression");
}

void codegen(Node *node) {
  printf("  .globl main\n");
  printf("main:\n");

  gen_expr(node);
  printf("  ret\n");

  assert(depth == 0);
}
