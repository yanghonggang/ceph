// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "include/objectstore/libobjectstore.h"
#include "common/ceph_argparse.h"
#include "common/common_init.h"
#include "common/ceph_context.h"

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

extern "C" int os_create(config_ctx_t cct, object_store_t *os)
{
  return 0;
}
