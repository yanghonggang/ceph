#include <stdio.h>
#include <string.h>

#include "objectstore/libobjectstore.h"

int main() {
  printf("Creating config context...\n");
  config_ctx_t ctx = config_ctx_create();

  if (ctx == NULL) {
    fprintf(stderr, "Failed to create config context\n");
    return 1;
  }
  printf("Config context created successfully: %p\n", (void*)ctx);

  int ret = -1;
  {
    object_store_t os;
    ret = os_create(ctx, "bluestore", "./os_test", &os);
    if (ret < 0) {
      fprintf(stderr, "os_create failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    }
    printf("ObjectStore created successfully: %p\n", (void*)os);

    ret = os_mkfs(os);
    if (ret < 0) {
      fprintf(stderr, "os_mkfs failed with error: %d (%s)\n", ret,
        strerror(-ret));
    } else {
      printf("ObjectStore os_mkfs successfully\n");
    }

    ret = os_destroy(os);
    if (ret < 0) {
      fprintf(stderr, "os_destroy failed with error: %d (%s)\n", ret,
        strerror(-ret));
    } else {
      printf("ObjectStore destroyed successfully\n");
    }

    ret = os_destroy(os);
    if (ret < 0) {
      printf("Second os_destroy correctly failed with error: %d (%s)\n", ret,
        strerror(-ret));
    }
  }

cleanup:
  printf("Destroying config context...\n");
  config_ctx_destroy(ctx);
  printf("Config context destroyed.\n");

  return ret;
}
