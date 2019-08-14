cmake_minimum_required(VERSION 3.0)
project(feov1725)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
   # in standalone mode, install under <midas>/bin
   set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/../bin CACHE PATH "" FORCE)
else()
   # there is an user choice or a higher level project
   # specifying the install path - keep that
   set(CMAKE_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
endif()

message(STATUS "feoV1725: Building feoV1725")


# Check for MIDASSYS environment variable
if (NOT DEFINED ENV{MIDASSYS})
   message(SEND_ERROR "MIDASSYS environment variable not defined.")
endif()

set(CMAKE_CXX_STANDARD 11)
set(MIDASSYS $ENV{MIDASSYS})


# Hardware setup
# Number of optical links used per A3818
set(NBLINKSPERA3818 1)

# Number of optical links controlled by each frontend 
set (NBLINKSPERFE 1) 

# Number of daisy-chained v1725s per optical link
set (NB1725PERLINK 1)

# Number of v1725 boards in total 
set (NBV1725TOTAL 1) 

# Number of cores
set (NBCORES 12)

# Using the SYSTEM buffer
set (USE_SYSTEM_BUFFER 1)

set(DRIVERS
#    ${MIDASSYS}/drivers/class/hv
#    ${MIDASSYS}/drivers/device/dd_sy4527
#    ${MIDASSYS}/drivers/bus/null

)

if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
   set(LIBS -ldl -lpthread -lutil -lrt -lm -lz -lnsl -lzmq)
   set(CAENLIBS -lCAENComm -lCAENVME)
endif()

add_executable(feov1725
  feoV1725
  v1725CONET2
  ov1725)

install(TARGETS feov1725 DESTINATION ${CMAKE_SOURCE_DIR}/../bin)

target_compile_options(feov1725 PRIVATE -DLINUX 
   -DUNIX 
   -DUSE_SYSTEM_BUFFER=${USE_SYSTEM_BUFFER} 
   -DNBLINKSPERA3818=${NBLINKSPERA3818}
   -DNBLINKSPERFE=${NBLINKSPERFE}
   -DNB1725PERLINK=${NB1725PERLINK}
   -DNBV1725TOTAL=${NBV1725TOTAL}
   -DNBCORES=${NBCORES}
)

target_include_directories(feov1725 PRIVATE
  ${MIDASSYS}/include
  ${MIDASSYS}/drivers
)

target_link_libraries(feov1725 ${MIDASSYS}/lib/libmfe.a ${MIDASSYS}/lib/libmidas.a ${CAENLIBS} ${LIBS})
