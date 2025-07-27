// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 *  Copyright (C) 2025 CMCC <yanghonggang_yewu@cmss.chinamobile.com>
 *
 * Author: Yang Honggang <yanghonggang_yewu@@cmss.chinamobile.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#ifndef CEPH_LIB_OS_H
#define CEPH_LIB_OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @typedef config_ctx_t
 *
 * A handle of the os configuration context.
 *
 */
typedef void *config_ctx_t;

/**
 * @typedef object_store_t
 *
 * A handle of ObjectStore.
 */
typedef void *object_store_t;

/**
 * @typedef collection_t
 *
 * A collection is simply a grouping of objects. Collections have
 * names (cid_t) and can be enumerated in order.
 */
typedef void *collection_t;

/**
 * @typedef cid_t
 *
 * Uniquely identify a single collection.
 */
typedef uint64_t cid_t;

/**
 * A Transaction represents a sequence of primitive mutation
 * operations.
 */
typedef void *transaction_t;

/**
 * Init ObjectStore configuration context.
 *
 * @return non-NULL on success, NULL on failure
 */
config_ctx_t config_ctx_create();

/**
 * Destroy a previously created configuration context.
 *
 * @param cct the configuration context to destroy.
 */
void config_ctx_destroy(config_ctx_t cct);

/**
 * Create an ObjectStore instance.
 *
 * @param cct ObjectStore configuration context
 * @param type type of store
 * @param data path (or other descriptor) for data
 * @param os where to store the ObjectStore handle
 * @returns 0 on success, negative error code on failure
 */
int os_create(config_ctx_t cct, const char* type, const char* data,
  object_store_t *os);

/**
 * Destroy an ObjectStore instance.
 *
 * This function destroys the specified ObjectStore instance. After calling this
 * function, the ObjectStore handle should no longer be used.
 *
 * @param os The handle of the ObjectStore instance to destroy. This handle must
 *           have been previously obtained from a successful call to os_create().
 * @returns 0 on success, negative error code on failure. If the ObjectStore
 *          instance is not found, -ENOENT is returned.
 */
int os_destroy(object_store_t os);

/**
 * Format ObjectStore's disk(s).
 *
 * @param os ObjectStore handle
 * @returns 0 on success, negative error code on failure
 */
int os_mkfs(object_store_t os);

/**
 * Load and initialize the ObjectStore.
 *
 * This function loads the metadata of the ObjectStore from disk(s),
 * initializes necessary internal structures, and prepares the service
 * to start accepting I/O requests.
 *
 * @param os ObjectStore handle
 * @returns 0 on success, negative error code on failure
 */
int os_mount(object_store_t os);

/**
 * Drain inprogress requests and sync ObjectStore metadata to disk(s).
 *
 * @param os ObjectStore handle
 * @returns 0 on success, negative error code on failure
 */
int os_umount(object_store_t os);

/**
 * Get a collection handle.
 *
 * @param os ObjectStore handle
 * @param cid id/name of the collection
 * @param c where to store the collection handle
 * @returns 0 on success, negative error code on failure
 */
int os_open_collection(object_store_t os, cid_t cid, collection_t *c);

/**
 * Create and initialize a new transaction object.
 *
 * This function allocates and initializes a new transaction object that can be
 * used to batch multiple operations into a single atomic transaction. The
 * returned transaction_t value should be passed to os_release_transaction()
 * when no longer needed.
 *
 * @note Once the transaction is submitted it must no longer be used
 *
 * @return non-NULL on success, NULL on memory allocation error
 */
transaction_t os_create_transaction();

/**
 * Release resources held by a transaction object.
 *
 * @param tx The transaction object to be released.
 */
void os_release_transaction(transaction_t tx);

/**
 * Get a collection handle for a soon-to-be-created collection.
 *
 * This handle must be used by os_queue_transaction that includes a
 * os_create_new_collection call in order to become valid. It will become the
 * reference to the created collection.
 *
 * @param os ObjectStore handle
 * @param cid id/name of the collection to be created
 * @param c where to store the collection handle
 * @return non-NULL on success, NULL on memory allocation error
 */
collection_t os_create_new_collection(object_store_t os, cid_t cid);

/**
 * Release a collection.
 *
 * @param coll_ A pointer to the collection previously obtained from
 *              `os_create_new_collection`. Passing NULL is allowed and results
 *              in no operation.
 */
void os_release_collection(collection_t coll);

/**
 * Add a collection creation operation to the specified transaction.
 *
 * @param tx transaction within which the collection creation will be executed
 * @param c collection handle returned by os_create_new_collection()
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_create_collection(transaction_t tx, collection_t c);

/**
 * Add an object write operation to the specified transaction.
 *
 * If the object is too small, it is expanded as needed. It is possible to
 * specify an offset beyond the current end of an object and it will be
 * expanded as needed. ObjectStore will omit the untouched data and store it as
 * a "hole" in the file.
 *
 * Note: a 0-length write does not affect the size of the object
 *
 * @param tx transaction to which the write operation will be added
 * @param cid id/name of the collection
 * @param oid object name within the collection
 * @param offset offset within the object where the write operation will begin
 * @param len length of buffer
 * @param data bytes to be written
 * @param flags flags specifying additional options for the write operation
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_object_write(transaction_t tx, cid_t cid, const char *oid,
                                 const char *data, uint64_t offset,
                                 uint64_t len, uint32_t flags);

/**
 * Submit a transaction for execution.
 *
 * This function queues the transaction to the ObjectStore for asynchronous
 * execution. Once submitted, the transaction must no longer be used or
 * modified.
 *
 * @param os ObjectStore handle
 * @param c collection handle associated with the transaction
 * @param tx transaction to queue
 * @returns 0 on success, negative error code on failure
 */
int os_queue_transaction(object_store_t os, collection_t c, transaction_t tx);

// FIXME: just for test
int os_collection_list(object_store_t os, collection_t c, const char **ls,
                       int cnt);

/**
 * Read a byte range of data from an object.
 *
 * Note: If reading from an offset past the end of the object, this function
 * returns 0, indicating no bytes were read (rather than returning an error code
 * such as -EINVAL).
 *
 * @param os ObjectStore handle
 * @param c collection handle containing the object
 * @param oid object identifier within the collection
 * @param offset location offset of the first byte to be read
 * @param len number of bytes to be read
 * @param buf buffer where the read data will be stored
 * @param flags Flags specifying additional options for the read operation
 * @returns Number of bytes read on success, or a negative error code on failure
 */
int os_object_read(object_store_t os, collection_t c, const char *oid,
                   uint64_t offset, uint64_t len, char *buf, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
