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

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

// Compute the absolute address of a given node.
// It's an error if a given node does not reside in memory.
static void gen_addr(Node *node) {
  if (node->kind == ND_VAR) {
    printf("  add x0, x29, %d\n", node->var->offset);
    return;
  }

  error("not an lvalue");
}

// Generate code for a given node.
static void gen_expr(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  ldr x0, =%d\n", node->val);
    return;
  case ND_NEG:
    gen_expr(node->lhs);
    printf("  neg x0, x0\n");
    return;
  case ND_VAR:
    gen_addr(node);
    printf("  ldr x0, [x0]\n");
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    pop("x1");
    printf("  str x0, [x1]\n");
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

static void gen_stmt(Node *node) {
  if (node->kind == ND_EXPR_STMT) {
    gen_expr(node->lhs);
    return;
  }

  error("invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Function *prog) {
  int offset = 0;
  for (Obj *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = -offset;
  }
  prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  printf("  .globl main\n");
  printf("main:\n");

  // Prologue
  printf("  stp x29, x30, [sp, #-16]!\n");
  printf("  mov x29, sp\n");
  printf("  sub sp, sp, #%d\n", prog->stack_size);

  for (Node *n = prog->body; n; n = n->next) {
    gen_stmt(n);
    assert(depth == 0);
  }

  printf("  add sp, sp, #%d\n", prog->stack_size);
  printf("  ldp x29, x30, [sp], #16\n");
  printf("  ret\n");
}
