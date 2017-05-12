CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#IF(COMMAND cmake_policy)
#  cmake_policy(SET CMP0003 NEW)
#ENDIF(COMMAND cmake_policy)  

FIND_PACKAGE(SoapySDR REQUIRED)
FIND_PACKAGE(Boost 
  COMPONENTS program_options thread date_time 
             system filesystem serialization regex)

set(CMAKE_BUILD_TYPE "Debug")


INCLUDE_DIRECTORIES(${SoapySDR_INCLUDE_DIRS})

ADD_EXECUTABLE(CxxAPITest CxxAPITest.cxx)

TARGET_LINK_LIBRARIES(CxxAPITest rt ${SoapySDR_LIBRARIES})

ADD_EXECUTABLE(WriteStreamTest WriteStreamTest.cxx)

TARGET_LINK_LIBRARIES(WriteStreamTest rt ${SoapySDR_LIBRARIES})


ADD_EXECUTABLE(TimeTest TimeTest.cxx)

TARGET_LINK_LIBRARIES(TimeTest rt ${SoapySDR_LIBRARIES})

