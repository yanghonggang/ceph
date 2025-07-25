add_library(objectstore SHARED
  libobjectstore_c.cc
)

target_link_libraries(objectstore PRIVATE os common ${CRYPTO_LIBS} ${ceph_common_deps})
#target_link_libraries(objectstore PRIVATE common os blk heap_profiler kv ${FMT_LIB} ${ceph_common_deps})
#  ${BLKID_LIBRARIES} ${CRYPTO_LIBS} ${EXTRALIBS} ${GSSAPI_LIBRARIES})

#target_link_options(objectstore PRIVATE
#    -static-libstdc++
#    -static-libgcc
#)

#set_property(TARGET objectstore APPEND_STRING PROPERTY
#  LINK_FLAGS " -static-libstdc++ -static-libgcc")

set_target_properties(objectstore PROPERTIES
  OUTPUT_NAME objectstore
  VERSION 1.0.0
  SOVERSION 1
  VISIBILITY_INLINES_HIDDEN ON)

install(TARGETS objectstore DESTINATION ${CMAKE_INSTALL_LIBDIR})
