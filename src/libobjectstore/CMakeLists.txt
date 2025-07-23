add_library(objectstore SHARED
  libobjectstore_c.cc
)

target_link_libraries(objectstore PRIVATE os common ${CRYPTO_LIBS} ${ceph_common_deps})
#target_link_libraries(objectstore PRIVATE common os blk heap_profiler kv ${FMT_LIB} ${ceph_common_deps})
#  ${BLKID_LIBRARIES} ${CRYPTO_LIBS} ${EXTRALIBS} ${GSSAPI_LIBRARIES})

install(TARGETS objectstore DESTINATION ${CMAKE_INSTALL_LIBDIR})
