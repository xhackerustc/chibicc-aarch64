#include "chibicc.h"

static int depth;
static char *argreg[] = {"x0", "x1", "x2", "x3", "x4", "x5"};
static Function *current_fn;

static void gen_expr(Node *node);

static int count(void) {
  static int i = 1;
  return i++;
}

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
  switch (node->kind) {
  case ND_VAR:
    printf("  add x0, x29, %d\n", node->var->offset);
    return;
  case ND_DEREF:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "not an lvalue");
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
  case ND_DEREF:
    gen_expr(node->lhs);
    printf("  ldr x0, [x0]\n");
    return;
  case ND_ADDR:
    gen_addr(node->lhs);
    return;
  case ND_ASSIGN:
    gen_addr(node->lhs);
    push();
    gen_expr(node->rhs);
    pop("x1");
    printf("  str x0, [x1]\n");
    return;
  case ND_FUNCALL: {
    int nargs = 0;
    for (Node *arg = node->args; arg; arg = arg->next) {
      gen_expr(arg);
      push();
      nargs++;
    }

    for (int i = nargs - 1; i >= 0; i--)
      pop(argreg[i]);

    printf("  bl %s\n", node->funcname);
    return;
  }
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

  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
  case ND_IF: {
    int c = count();
    gen_expr(node->cond);
    printf("  cbz x0, .L.else.%d\n", c);
    gen_stmt(node->then);
    printf("  b .L.end.%d\n", c);
    printf(".L.else.%d:\n", c);
    if (node->els)
      gen_stmt(node->els);
    printf(".L.end.%d:\n", c);
    return;
  }
  case ND_FOR: {
    int c = count();
    if (node->init)
      gen_stmt(node->init);
    printf(".L.begin.%d:\n", c);
    if (node->cond) {
      gen_expr(node->cond);
      printf("  cbz x0, .L.end.%d\n", c);
    }
    gen_stmt(node->then);
    if (node->inc)
      gen_expr(node->inc);
    printf("  b .L.begin.%d\n", c);
    printf(".L.end.%d:\n", c);
    return;
  }
  case ND_BLOCK:
    for (Node *n = node->body; n; n = n->next)
      gen_stmt(n);
    return;
  case ND_RETURN:
    gen_expr(node->lhs);
    printf("  b .L.return.%s\n", current_fn->name);
    return;
  case ND_EXPR_STMT:
    gen_expr(node->lhs);
    return;
  }

  error_tok(node->tok, "invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Function *prog) {
  for (Function *fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += 8;
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  for (Function *fn = prog; fn; fn = fn->next) {
    printf("  .globl %s\n", fn->name);
    printf("%s:\n", fn->name);
    current_fn = fn;

    // Prologue
    printf("  stp x29, x30, [sp, #-16]!\n");
    printf("  mov x29, sp\n");
    printf("  sub sp, sp, #%d\n", fn->stack_size);

    // Save passed-by-register arguments to the stack
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next)
      printf("  str %s, [x29, #%d]\n", argreg[i++], var->offset);

    // Emit code
    gen_stmt(fn->body);
    assert(depth == 0);

    // Epilogue
    printf(".L.return.%s:\n", fn->name);
    printf("  add sp, sp, #%d\n", fn->stack_size);
    printf("  ldp x29, x30, [sp], #16\n");
    printf("  ret\n");
  }
}
