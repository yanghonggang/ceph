// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "include/objectstore/libobjectstore.h"
#include "common/ceph_argparse.h"
#include "common/common_init.h"
#include "common/ceph_context.h"
#include "os/ObjectStore.h"

namespace {

std::mutex store_mutex;
std::unordered_map<ObjectStore*, std::unique_ptr<ObjectStore>> store_instances;

} // anonymous namespace

static CephContext *create_cct(const char * const clustername,
  CephInitParameters *iparams)
{
  // missing things compared to global_init:
  // g_ceph_context, g_conf, g_lockdep, signal handlers
  CephContext *cct = common_preinit(*iparams, CODE_ENVIRONMENT_LIBRARY, 0);
  if (clustername)
    cct->_conf->cluster = clustername;
  cct->_conf.parse_env(cct->get_module_type()); // environment variables override
  cct->_conf.apply_changes(nullptr);

  // FIXME: global_init_set_globals(cct);
  g_ceph_context = cct;

  return cct;
}

static inline cid_t get_cid_t(const coll_t& coll)
{
  return coll.pool();
}

extern "C" config_ctx_t config_ctx_create()
{
  CephInitParameters iparams(CEPH_ENTITY_TYPE_CLIENT);
  CephContext *cct = create_cct("", &iparams);
  return static_cast<config_ctx_t>(cct);
}

extern "C" void config_ctx_destroy(config_ctx_t cct_)
{
  CephContext *cct = static_cast<CephContext*>(cct_);
  if (cct) {
    cct->put();
  }
}

extern "C" int os_create(config_ctx_t cct_, const char* type, const char* data,
  object_store_t *os_)
{
  std::lock_guard<std::mutex> lock(store_mutex);
  CephContext *cct = static_cast<CephContext*>(cct_);
  std::unique_ptr<ObjectStore> os = ObjectStore::create(cct, type, data, "", 0);
  if (!os) {
    std::cerr << "unable to create store of type " << type << std::endl;
    return -1;
  }

  ObjectStore* raw = os.get();
  store_instances[raw] = std::move(os);
  *os_ = static_cast<void*>(raw);

  return 0;
}

extern "C" int os_destroy(object_store_t os_)
{
  if (!os_) {
    return -EINVAL;
  }

  ObjectStore* os = static_cast<ObjectStore*>(os_);

  std::lock_guard<std::mutex> lock(store_mutex);

  auto it = store_instances.find(os);
  if (it != store_instances.end()) {
    store_instances.erase(it);
    return 0;
  }

  return -ENOENT;
}

extern "C" int os_mkfs(object_store_t os_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return -EINVAL;
  }

  int ret = os->mkfs();
  if (ret) {
    std::cerr << "mkfs failed: ret=" << ret << std::endl;
  }

  return 0;
}

extern "C" int os_mount(object_store_t os_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return -EINVAL;
  }

  int ret = os->mount();
  if (ret) {
    std::cerr << "mount failed: ret=" << ret << std::endl;
  }

  return 0;
}

extern "C" int os_umount(object_store_t os_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return -EINVAL;
  }

  int ret = os->umount();
  if (ret) {
    std::cerr << "umount failed: ret=" << ret << std::endl;
  }

  return 0;
}

static void fill_store_statfs(os_statfs_t *buf, const store_statfs_t &stats) {
  buf->total = stats.total;
  buf->available = stats.available;
  buf->internally_reserved = stats.internally_reserved;
  buf->allocated = stats.allocated;
  buf->data_stored = stats.data_stored;
  buf->data_compressed = stats.data_compressed;
  buf->data_compressed_allocated = stats.data_compressed_allocated;
  buf->data_compressed_original = stats.data_compressed_original;
  buf->omap_allocated = stats.omap_allocated;
  buf->internal_metadata = stats.internal_metadata;
}

extern "C" int os_statfs(object_store_t os_, os_statfs_t *buf)
{
  if (!buf) {
    std::cerr << "Buffer is null" << std::endl;
    return -EINVAL;
  }

  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "ObjectStore instance is null" << std::endl;
    return -EINVAL;
  }

  store_statfs_t stats;
  int result = os->statfs(&stats);
  if (result != 0) {
    return result;
  }

  fill_store_statfs(buf, stats);

  return 0;
}

extern "C" int os_pool_statfs(object_store_t os_, uint64_t pool_id,
  os_statfs_t *buf)
{
  if (!buf) {
    std::cerr << "Buffer is null" << std::endl;
    return -EINVAL;
  }

  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "ObjectStore instance is null" << std::endl;
    return -EINVAL;
  }

  store_statfs_t stats;
  bool per_pool_omap;
  int result = os->pool_statfs(pool_id, &stats, &per_pool_omap);
  if (result != 0) {
    return result;
  }

  fill_store_statfs(buf, stats);

  return 0;
}

#define PG_ID (0)

struct C_Collection {
  ObjectStore::CollectionHandle ch;
};

static inline coll_t get_coll_t(cid_t cid_)
{
  ps_t pg = PG_ID;
  int64_t poolid = cid_;
  coll_t cid = coll_t(spg_t(pg_t(pg, poolid), shard_id_t::NO_SHARD));
  std::cout << "cid=" << cid << std::endl;

  return cid;
}

extern "C" collection_t os_create_new_collection(object_store_t os_, cid_t cid_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return nullptr;
  }

  coll_t cid = get_coll_t(cid_);
  ObjectStore::CollectionHandle ch = os->create_new_collection(cid);
  if (!ch) {
    std::cerr << "create_new_collection failed" << std::endl;
    return nullptr;
  }

  C_Collection *cc = new C_Collection;
  cc->ch = ch;

  return static_cast<collection_t>(cc);
}

extern "C" void os_release_collection(collection_t coll)
{
  C_Collection *cc = static_cast<C_Collection*>(coll);
  if (!cc || !cc->ch) {
    return;
  }
  delete cc;
}

extern "C" collection_t os_open_collection(object_store_t os_, cid_t cid_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return nullptr;
  }

  coll_t cid = get_coll_t(cid_);
  ObjectStore::CollectionHandle ch = os->open_collection(cid);
  if (!ch) {
    std::cerr << "os_open_collection failed" << std::endl;
    return nullptr;
  }

  C_Collection *cc = new C_Collection;
  if (!cc) {
    return nullptr;
  }

  cc->ch = ch;

  return static_cast<collection_t>(cc);
}

static inline ghobject_t get_ghobject_t(const char *oid, uint64_t pool)
{
  return ghobject_t(hobject_t(oid, "", CEPH_NOSNAP, 0, pool, ""));
}

extern "C" int os_object_read(object_store_t os_, collection_t coll,
  const char *oid, uint64_t offset, uint64_t len, char *buf, uint32_t flags)
{
  C_Collection *cc = static_cast<C_Collection*>(coll);
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os || !cc || !cc->ch || !buf) {
    std::cerr << "os or c/ch or buf is null" << std::endl;
    return -EINVAL;
  }

  bufferlist bl;
  bufferptr bp = buffer::create_static(len, buf);
  bl.push_back(bp);

  coll_t cid = cc->ch->cid;
  // ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
  auto hoid = get_ghobject_t(oid, cid.pool());
  int ret = os->read(cc->ch, hoid, offset, len, bl);
  if (ret >= 0) {
    if (bl.length() > len) {
      return -ERANGE;
    }
    if (!bl.is_provided_buffer(buf)) {
      bl.begin().copy(bl.length(), buf);
    }

    ret = bl.length();
  }

  return ret;
}

struct C_Transaction {
  std::unique_ptr<ObjectStore::Transaction> tx;
};

extern "C" transaction_t os_create_transaction()
{
  C_Transaction* ct = new C_Transaction();
  ct->tx = std::make_unique<ObjectStore::Transaction>();
  return static_cast<transaction_t>(ct);
}

extern "C" void os_release_transaction(transaction_t tx_)
{
  if (!tx_) {
    return;
  }

  C_Transaction* ct = static_cast<C_Transaction*>(tx_);
  delete ct;
}

extern "C" int os_transaction_collection_create(transaction_t tx,
  collection_t coll)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  C_Collection *cc = static_cast<C_Collection*>(coll);
   if (!ct || !ct->tx || !cc || !cc->ch) {
    return -EINVAL;
  }

  int split_bits = 0;
  ct->tx->create_collection(cc->ch->cid, split_bits);

  return 0;
}

extern "C" int os_transaction_collection_remove(transaction_t tx, cid_t cid_)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  ct->tx->remove_collection(cid);

  return 0;
}

extern "C" int os_transaction_object_write(transaction_t tx, cid_t cid_,
  const char *oid, const char *data, uint64_t offset, uint64_t len,
  uint32_t flags)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx || !oid || !data) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  // ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
  auto hoid = get_ghobject_t(oid, cid.pool());
  bufferlist bl;
  bl.append(data);

  ct->tx->write(cid, hoid, offset, len, bl, flags);

  return 0;
}

extern "C" int os_transaction_object_zero(transaction_t tx, cid_t cid_,
  const char *oid, uint64_t off, uint64_t len)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx || !oid) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  // ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
  auto hoid = get_ghobject_t(oid, cid.pool());

  ct->tx->zero(cid, hoid, off, len);

  return 0;
}

extern "C" int os_transaction_object_remove(transaction_t tx, cid_t cid_,
  const char *oid)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx || !oid) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  // ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
  auto hoid = get_ghobject_t(oid, cid.pool());

  ct->tx->remove(cid, hoid);

  return 0;
}

extern "C" int os_transaction_object_rename(transaction_t tx, cid_t cid_,
  const char *oldoid, const char *oid)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx || !oid || !oldoid || cid_ == LIBOS_CID_INVALID) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  auto pool = cid.pool();
  // ghobject_t hoid_old(hobject_t(oldoid, "", CEPH_NOSNAP, 0, pool, ""));
  // ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, pool, ""));

  auto hoid_old = get_ghobject_t(oldoid, pool);
  auto hoid = get_ghobject_t(oid, pool);

  std::cout << "rename from " << hoid_old << " to " << hoid << std::endl;

  ct->tx->collection_move_rename(cid, hoid_old, cid, hoid);

  return 0;
}

extern "C" int os_queue_transaction(object_store_t os_, collection_t coll,
  transaction_t tx)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  C_Collection *cc = static_cast<C_Collection*>(coll);
  if (!os_ || !ct || !ct->tx || !cc || !cc->ch) {
    return -EINVAL;
  }
  ObjectStore* os = static_cast<ObjectStore*>(os_);

  os->queue_transaction(cc->ch, std::move(*ct->tx));

  return 0;
}

extern "C" int os_collection_list(object_store_t os_, cid_t start, cid_t *cids,
  int cnt, cid_t *next)
{
  if (!os_ || !cids || !next || cnt < 0) {
    if (next) *next = LIBOS_CID_INVALID;
    return -EINVAL;
  }

  ObjectStore* os = static_cast<ObjectStore*>(os_);
  std::vector<coll_t> ls;

  int r = os->list_collections(ls);
  if (r < 0) {
    *next = LIBOS_CID_INVALID;
    return r;
  }

  if (ls.empty()) {
    *next = LIBOS_CID_INVALID;
    return 0;
  }

  std::vector<coll_t>::const_iterator it;
  if (start == LIBOS_CID_INVALID) {
    it = ls.begin();
  } else {
    coll_t start_coll = get_coll_t(start);
    it = std::lower_bound(ls.begin(), ls.end(), start_coll);
    if (it == ls.end()) {
      *next = LIBOS_CID_INVALID;
      return 0;
    }
  }

  int filled = 0;
  while (it != ls.end() && filled < cnt) {
    cids[filled] = get_cid_t(*it);
    ++filled;
    ++it;
  }

  if (it != ls.end()) {
    *next = get_cid_t(*it);
  } else {
    *next = LIBOS_CID_INVALID;
  }

  return filled;
}

extern "C" int os_object_list(object_store_t os_, collection_t c,
  const char* start, const char* end, int max, char* buf, uint64_t* buf_len,
  int* count, char* next, uint64_t* next_len)
{
  C_Collection *cc = static_cast<C_Collection*>(c);
  ObjectStore* os = static_cast<ObjectStore*>(os_);

  if (!os || !cc || !cc->ch || !buf_len || !count || !next || !next_len) {
    return -EINVAL;
  }

  *count = 0;
  next[0] = '\0';

  coll_t cid = cc->ch->cid;
  auto pool = cid.pool();

  auto start_oid = start ? get_ghobject_t(start, pool) : ghobject_t();
  auto end_oid = end ? get_ghobject_t(end, pool) : ghobject_t::get_max();

  std::vector<ghobject_t> ls;
  ghobject_t next_ghobj;
  int ret = os->collection_list(cc->ch, start_oid, end_oid, max, &ls,
    &next_ghobj);
  if (ret < 0) {
    return ret;
  }

  size_t needed = 0;
  for (const auto& obj : ls) {
      const std::string& key = obj.hobj.get_effective_key();
      std::cout << __func__ << " obj=" << obj << " | hobj=" << obj.hobj << "| key=" << key << std::endl;
      needed += key.size() + 1; // +1 for '\0'
  }
  *count = ls.size();

  if (buf && *buf_len < needed) {
      *buf_len = needed;
      return -ENOSPC;
  }

  if (buf && *buf_len >= needed) {
      char* p = buf;
      for (const auto& obj : ls) {
          const std::string& key = obj.hobj.get_effective_key();
          strncpy(p, key.c_str(), key.size());
          p[key.size()] = '\0';
          p += key.size() + 1;
      }
      *buf_len = needed;
  } else {
      *buf_len = needed;
  }

  if (!next_ghobj.is_max()) {
      const std::string& next_str = next_ghobj.hobj.get_key();
      if (next_str.size() + 1 <= *next_len) {
          strncpy(next, next_str.c_str(), next_str.size());
          next[next_str.size()] = '\0';
          *next_len = next_str.size() + 1;
      } else {
          *next_len = next_str.size() + 1;
          return -ENOSPC;
      }
  } else {
      *next_len = 1;
      next[0] = '\0';
  }

  return 0;
}
