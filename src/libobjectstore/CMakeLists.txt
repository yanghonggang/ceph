add_library(objectstore SHARED
  libobjectstore_c.cc
)

target_link_libraries(objectstore PRIVATE ceph-common)

#  ${BLKID_LIBRARIES} ${CRYPTO_LIBS} ${EXTRALIBS} ${GSSAPI_LIBRARIES})

install(TARGETS objectstore DESTINATION ${CMAKE_INSTALL_LIBDIR})
