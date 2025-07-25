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
  printf("#Config context created successfully: %p\n", (void*)ctx);

  int ret = -1;
  {
    object_store_t os = NULL;
    ret = os_create(ctx, "bluestore", "./os_test", &os);
    if (ret < 0) {
      fprintf(stderr, "os_create failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    }
    printf("#ObjectStore created successfully: %p\n", (void*)os);

    ret = os_mkfs(os);
    if (ret < 0) {
      fprintf(stderr, "os_mkfs failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    } else {
      printf("#ObjectStore os_mkfs successfully\n");
    }

    ret = os_mount(os);
    if (ret < 0) {
      printf("os_mount failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    }

    printf("#ObjectStore mount successfully: %p\n", (void*)os);

    ret = os_umount(os);
    if (ret < 0) {
      printf("os_mount failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    }
    printf("#ObjectStore umount successfully: %p\n", (void*)os);

#if 0
    ret = os_destroy(os);
    if (ret < 0) {
      fprintf(stderr, "os_destroy failed with error: %d (%s)\n", ret,
        strerror(-ret));
        goto cleanup;
    } else {
      printf("#ObjectStore destroyed successfully\n");
    }

    ret = os_destroy(os);
    if (ret < 0) {
      printf("Second os_destroy correctly failed with error: %d (%s)\n", ret,
        strerror(-ret));
    }
    os = NULL;
#endif
  }

cleanup:
  config_ctx_destroy(ctx);
  printf("#Config context destroyed.\n");

  return ret;
}
