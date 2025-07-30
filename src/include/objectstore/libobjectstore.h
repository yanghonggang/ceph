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
 * Represents an invalid, used as a starting point for listing collections or
 * indicating no valid collection ID is available.
 */
#define LIBOS_CID_INVALID ((cid_t)(-1))

/**
 * A Transaction represents a sequence of primitive mutation
 * operations.
 */
typedef void *transaction_t;

/**
 * ObjectStore full statfs information
 */
typedef struct
{
  uint64_t total;                  ///< Total bytes
  uint64_t available;              ///< Free bytes available
  uint64_t internally_reserved;    ///< Bytes reserved for internal purposes

  int64_t allocated;               ///< Bytes allocated by the store

  int64_t data_stored;                ///< Bytes actually stored by the user
  int64_t data_compressed;            ///< Bytes stored after compression
  int64_t data_compressed_allocated;  ///< Bytes allocated for compressed data
  int64_t data_compressed_original;   ///< Bytes that were compressed

  int64_t omap_allocated;         ///< approx usage of omap data
  int64_t internal_metadata;      ///< approx usage of internal metadata
} os_statfs_t;

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
 * Get statfs information of ObjectStore
 *
 * @param os ObjectStore handle
 * @param buf where to store statfs info
 * @returns 0 on success, negative error code on failure
 */
int os_statfs(object_store_t os, os_statfs_t *buf);

/**
 * Get statfs information of ObjectStore's pool
 *
 * @param os ObjectStore handle
 * @param pool_id id of the pool
 * @param buf where to store statfs info
 * @returns 0 on success, negative error code on failure
 */
int os_pool_statfs(object_store_t os, uint64_t pool_id, os_statfs_t *buf);

/**
 * Get a collection handle.
 *
 * @param os ObjectStore handle
 * @param cid id/name of the collection
 * @return non-NULL on success, NULL on memory allocation error
 */
collection_t os_open_collection(object_store_t os, cid_t cid);

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
 *  `os_create_new_collection`. Passing NULL is allowed and results in no
 *  operation.
 */
void os_release_collection(collection_t coll);

/**
 * Add a collection creation operation to the specified transaction.
 *
 * @param tx transaction within which the collection creation will be executed
 * @param c collection handle returned by os_create_new_collection()
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_collection_create(transaction_t tx, collection_t c);

/**
 * Remove the collection, the collection must be empty.
 *
 * @param tx transaction within which the collection op will be executed
 * @param cid id/name of the collection to be removed
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_collection_remove(transaction_t tx, cid_t cid);

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
 * Zero out the indicated byte range within an object.
 *
 * ObjectStore instances may optimize this to release the underlying storage
 * space. If the zero range extends beyond the end of the object, the object
 * size is extended, just as if we were writing a buffer full of zeros.
 * EXCEPT if the length is 0, in which case (just like a 0-length write)
 * we do not adjust the object size.
 *
 * @param tx transaction to which the write operation will be added
 * @param cid id/name of the collection
 * @param oid object name within the collection
 * @param offset offset within the object where the write operation will begin
 * @param len length of buffer
 * @param data bytes to be written
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_object_zero(transaction_t tx, cid_t cid, const char *oid,
  uint64_t off, uint64_t len);

/**
 * Remove an object. All four parts of the object are removed.
 *
 * @param tx transaction to which the write operation will be added
 * @param cid id/name of the collection
 * @param oid object name within the collection
 * @param offset offset within the object where the write operation will begin
 * @param len length of buffer
 * @param data bytes to be written
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_object_remove(transaction_t tx, cid_t cid, const char *oid);

/**
 * Rename an object.
 *
 * @param tx transaction to which the write operation will be added
 * @param cid id/name of the collection
 * @param oldoid object current name
 * @param oid object new name
 * @returns 0 on success, negative error code on failure
 */
int os_transaction_object_rename(transaction_t tx, cid_t cid,
  const char *oldoid, const char *oid);

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

/**
 * List collections from the object store with pagination support.
 *
 * Note: If the start collection is not found or beyond the last collection,
 * this function returns 0, indicating no collections were found.
 *
 * @param os ObjectStore handle
 * @param start collection ID to start listing from; LIBOS_CID_INVALID means
 *  start from beginning
 * @param cids Buffer to store collection IDs (must be pre-allocated by
 *  caller)
 * @param max maximum number of collection IDs that can be stored in cids
 * @param next pointer to receive the next collection ID for pagination;
 *  LIBOS_CID_INVALID indicates no more collections.
 * @returns Number of collections written to cids (>= 0) on success,
 *  negative error code on failure.
 */
int os_collection_list(object_store_t os, cid_t start, cid_t *cids, int max,
  cid_t *next);

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

/**
 * Lists contents of a collection that fall within the range [start, end) and
 * returns no more than a specified number of results. This function writes all
 * keys into a single, contiguous buffer (buf), separated by '\0' characters.
 *
 * @note "next_len == 1 && next[0] = '\0'" indicates that the listing has
 *  reached the end, no more items are available.
 *
 * @param os ObjectStore handle
 * @param c collection handle containing the objects
 * @param start the starting key for listing objects; list object that sort >=
 *  this value. If start is NULL, listing begins from the very first object in
 *  the collection.
 * @param end the ending key for listing objects; list objects that sort < this
 *  value. If end is NULL, listing continues to the very last object in the
 *  collection (unbounded end).
 * @param max The maximum number of results to return
 * @param buf a pointer to a buffer where the keys will be written, separated by
 *  '\0'. If buf is NULL, the function calculates the required size for buf and
 *  returns -ENOSPC.
 * @param buf_len a pointer to the size of the buf on input, and the amount of
 *  buf used on output. If buf is too small, buf_len will contain the required
 *  size.
 * @param count a pointer to an integer where the actual number of items written
 *  to buf will be stored. If buf is NULL, count will not be modified.
 * @param next a pointer to a buffer where the next key after the last returned
 *  key will be stored. This can be used in subsequent calls to retrieve the
 *  next set of items.
 * @param next_len a pointer to the size of the next buffer on input, and the
 *  amount of next buffer used on output. If next is too small to hold the next
 *  key, next_len will contain the required size.
 * @returns 0 on success, negative error code on failure
  */
int os_object_list(object_store_t os, collection_t c, const char* start,
  const char* end, int max, char* buf, uint64_t* buf_len, int* count,
  char* next, uint64_t* next_len);

#ifdef __cplusplus
}
#endif

#endif
