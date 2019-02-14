/* Copied from https://github.com/ramonza/libcoro */
#include <stdio.h>

#include "coro.h"

coro_context ctx, mainctx;
struct coro_stack stack;

void coro_body(void *arg) {
    printf("OK\n");
    coro_transfer(&ctx, &mainctx);
    printf("Back in coro\n");
    coro_transfer(&ctx, &mainctx);
}

int main(int argc, char **argv) {
    coro_create(&mainctx, NULL, NULL, NULL, 0);
    coro_stack_alloc(&stack, 0);
    coro_create(&ctx, coro_body, NULL, stack.sptr, stack.ssze);
    printf("Created a coro\n");
    coro_transfer(&mainctx, &ctx);
    printf("Back in main\n");
    coro_transfer(&mainctx, &ctx);
    printf("Back in main again\n");
    return 0;
}
