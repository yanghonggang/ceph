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

    {
      cid_t cid = 12345;
      collection_t coll = os_create_new_collection(os, cid);
      if (!coll) {
        fprintf(stderr, "os_create_new_collection failed\n");
        goto umount;
      }
      printf("#Collection created successfully: %p\n", (void*)coll);

      transaction_t tx = os_create_transaction();
      if (!tx) {
        fprintf(stderr, "os_transaction_create failed\n");
        goto release_coll;
      }
      printf("#Transaction created successfully: %p\n", (void*)tx);

      ret = os_transaction_create_collection(tx, coll);
      if (ret < 0) {
        fprintf(stderr, "os_transaction_create_collection failed: %d (%s)\n", ret, strerror(-ret));
        goto release_tx;
      }
      printf("#Transaction: create collection successfully\n");

      ret = os_queue_transaction(os, coll, tx);
      if (ret < 0) {
        fprintf(stderr, "os_queue_transaction failed: %d (%s)\n", ret, strerror(-ret));
        goto release_tx;
      }
      printf("#Transaction queued successfully\n");

release_tx:
      os_release_transaction(tx);
      printf("#Transaction destroyed successfully: %p\n", (void*)tx);

release_coll:
      os_release_collection(coll);
      printf("#Collection released successfully: %p\n", (void*)coll);
    }

umount:
    ret = os_umount(os);
    if (ret < 0) {
      printf("os_mount failed with error: %d (%s)\n", ret,
        strerror(-ret));
      goto cleanup;
    }
    printf("#ObjectStore umount successfully: %p\n", (void*)os);

    ret = os_destroy(os);
    if (ret < 0) {
      fprintf(stderr, "os_destroy failed with error: %d (%s)\n", ret,
        strerror(-ret));
        goto cleanup;
    } else {
      printf("#ObjectStore destroyed successfully: %p\n", (void*)os);
    }

    ret = os_destroy(os);
    if (ret < 0) {
      printf("Second os_destroy correctly failed with error: %d (%s)\n", ret,
        strerror(-ret));
    }
    os = NULL;
  }

cleanup:
  config_ctx_destroy(ctx);
  printf("#Config context destroyed: %p\n", (void*)ctx);

  return ret;
}
