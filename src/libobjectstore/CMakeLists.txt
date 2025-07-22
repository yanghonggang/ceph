add_library(objectstore SHARED
  libobjectstore_c.cc
)

install(TARGETS objectstore DESTINATION ${CMAKE_INSTALL_LIBDIR})
