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

      const char* oid = "mytestobj";
      const char* data = oid;
      uint64_t offset = 0;
      uint64_t len = strlen(oid);
      uint32_t flags = 0;
      ret = os_transaction_object_write(tx, cid, oid, data, offset, len, flags);
      if (ret < 0) {
        fprintf(stderr, "os_transaction_object_write failed: %d (%s)\n", ret, strerror(-ret));
        goto release_tx;
      }
      printf("#Transaction: object write successfully\n");

      ret = os_queue_transaction(os, coll, tx);
      if (ret < 0) {
        fprintf(stderr, "os_queue_transaction failed: %d (%s)\n", ret, strerror(-ret));
        goto release_tx;
      }
      printf("#Transaction queued successfully\n");

      char read_buffer[1024];
      memset(read_buffer, 0, sizeof(read_buffer));
      ret = os_object_read(os, coll, oid, offset, len, read_buffer, flags);
      if (ret < 0) {
        fprintf(stderr, "os_object_read failed: %d (%s)\n", ret, strerror(-ret));
        goto release_tx;
      }
      printf("#Object read successfully: %s\n", read_buffer);

release_tx:
      os_release_transaction(tx);
      printf("#Transaction destroyed successfully: %p\n", (void*)tx);

      {
        transaction_t tx2 = os_create_transaction();
        if (!tx2) {
          fprintf(stderr, "os_transaction_create failed for tx2\n");
          goto release_coll;
        }
        printf("#Transaction tx2 created successfully: %p\n", (void*)tx2);

        const char* oid = "mytestobj";
        cid_t cid = 12345;

        int ret = os_transaction_object_zero(tx2, cid, oid, 2, 4);
        if (ret < 0) {
          fprintf(stderr, "os_transaction_object_zero failed in tx2: %d (%s)\n", ret, strerror(-ret));
          goto release_tx2;
        }
        printf("#Transaction tx2: object zero successfully\n");

        ret = os_queue_transaction(os, coll, tx2);
        if (ret < 0) {
          fprintf(stderr, "os_queue_transaction failed for tx2: %d (%s)\n", ret, strerror(-ret));
          goto release_tx2;
        }
        printf("#Transaction tx2 queued successfully\n");

        char read_buffer[1024];
        memset(read_buffer, 0, sizeof(read_buffer));
        ret = os_object_read(os, coll, oid, 0, sizeof(read_buffer), read_buffer, 0);
        if (ret < 0) {
          fprintf(stderr, "os_object_read failed: %d (%s)\n", ret, strerror(-ret));
          goto release_tx2;
        }
        printf("#Object read successfully (raw): '");
        for (int i = 0; i < strlen("mytestobj"); i++) {
          if (read_buffer[i] == '\0') {
            printf("\\0");
          } else {
            printf("%c", read_buffer[i]);
          }
        }
        printf("'\n");

        printf("#Object read as string: '%s'\n", read_buffer);

release_tx2:
        os_release_transaction(tx2);
        printf("#Transaction tx2 destroyed successfully: %p\n", (void*)tx2);
    }

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
