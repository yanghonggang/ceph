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
  ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
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

extern "C" int os_transaction_object_write(transaction_t tx, cid_t cid_,
  const char *oid, const char *data, uint64_t offset, uint64_t len,
  uint32_t flags)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
  if (!ct || !ct->tx || !oid || !data) {
    return -EINVAL;
  }

  coll_t cid = get_coll_t(cid_);
  ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));
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
  ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));

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
  ghobject_t hoid(hobject_t(oid, "", CEPH_NOSNAP, 0, cid.pool(), ""));

  ct->tx->remove(cid, hoid);

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
