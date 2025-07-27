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

extern "C" collection_t os_create_new_collection(object_store_t os_, cid_t cid_)
{
  ObjectStore* os = static_cast<ObjectStore*>(os_);
  if (!os) {
    std::cerr << "os is null" << std::endl;
    return nullptr;
  }

  // FIXME: 增加一个根据 pool/pg 生成 coll_t 的辅助函数
  ps_t pg = PG_ID;
  int64_t poolid = cid_;
  coll_t cid = coll_t(spg_t(pg_t(pg, poolid), shard_id_t::NO_SHARD));
  std::cout << "cid=" << cid << std::endl;
  ObjectStore::CollectionHandle ch = os->create_new_collection(cid);
  if (!ch) {
    std::cerr << "create_new_collection failed" << std::endl;
    return nullptr;
  }

  return static_cast<void*>(ch.detach());
}

extern "C" void os_release_collection(collection_t coll_)
{
  if (!coll_) {
    return;
  }

  ObjectStore::CollectionImpl *coll =
    static_cast<ObjectStore::CollectionImpl*>(coll_);
  coll->put();
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

extern "C" int os_transaction_create_collection(transaction_t tx,
  collection_t coll_)
{
  C_Transaction* ct = static_cast<C_Transaction*>(tx);
   if (!ct || !ct->tx || !coll_) {
    return -EINVAL;
  }

  ObjectStore::CollectionImpl *coll =
    static_cast<ObjectStore::CollectionImpl*>(coll_);

  int split_bits = 0;
  ct->tx->create_collection(coll->cid, split_bits);

  return 0;
}
