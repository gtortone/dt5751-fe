cmake_minimum_required(VERSION 3.0)
project(feodt5751)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   # in standalone mode, install under <midas>/bin
   set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../bin CACHE PATH "" FORCE)
else()
   # there is an user choice or a higher level project
   # specifying the install path - keep that
   set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

message(STATUS "feoDT5751: Building feoDT5751")


# Check for MIDASSYS environment variable
if (NOT DEFINED ENV{MIDASSYS})
   message(SEND_ERROR "MIDASSYS environment variable not defined.")
endif()

set(CMAKE_CXX_STANDARD 11)
set(MIDASSYS $ENV{MIDASSYS})

# Hardware setup
# Number of optical links used per A3818
set(NBLINKSPERA3818 4)

# Number of optical links controlled by each frontend 
set (NBLINKSPERFE 4) 

# Number of daisy-chained DT5751s per optical link
set (NBDT5751PERLINK 1)

# Number of DT5751 boards in total 
set (NBDT5751TOTAL 4) 

# Number of cores
set (NBCORES 4)

# Using the SYSTEM buffer
set (USE_SYSTEM_BUFFER 1)

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
   set(LIBS -ldl -lpthread -lutil -lrt -lm -lz -lnsl -lzmq)
   set(CAENLIBS -lCAENComm -lCAENVME)
endif()

add_executable(feodt5751
  feoDT5751
  dt5751CONET2
  odt5751)

install(TARGETS feodt5751 DESTINATION ${CMAKE_SOURCE_DIR}/../bin)

target_compile_options(feodt5751 PRIVATE -DLINUX 
   -DUNIX 
   -DUSE_SYSTEM_BUFFER=${USE_SYSTEM_BUFFER} 
   -DNBLINKSPERA3818=${NBLINKSPERA3818}
   -DNBLINKSPERFE=${NBLINKSPERFE}
   -DNBDT5751PERLINK=${NBDT5751PERLINK}
   -DNBDT5751TOTAL=${NBDT5751TOTAL}
   -DNBCORES=${NBCORES}
)

target_include_directories(feodt5751 PRIVATE
  ${MIDASSYS}/include
  ${MIDASSYS}/drivers
)

target_link_libraries(feodt5751 ${MIDASSYS}/lib/libmfe.a ${MIDASSYS}/lib/libmidas.a ${CAENLIBS} ${LIBS})
