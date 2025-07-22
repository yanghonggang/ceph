#include <stdio.h>

#include "objectstore/libobjectstore.h"

int main() {
  printf("Creating config context...\n");
  config_ctx_t ctx = config_ctx_create();

  if (ctx == NULL) {
    fprintf(stderr, "Failed to create config context\n");
    return 1;
  }

  printf("Config context created successfully: %p\n", (void*)ctx);

  printf("Destroying config context...\n");
  config_ctx_destroy(ctx);
  printf("Config context destroyed.\n");

  return 0;
}
