#set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_C_FLAGS "-g -O2")
IF(APPLE)
  set(CMAKE_CXX_FLAGS "-std=c++11 -stdlib=libc++ -g -O2")
ELSE()
  set(CMAKE_CXX_FLAGS "-std=gnu++0x")
ENDIF()

file(GLOB_RECURSE
    source_files
    *.h
    *.m
    *.c
)

SET_SOURCE_FILES_PROPERTIES(
	${source_files}
    PROPERTIES LANGUAGE C
)

file(COPY
kvdb.h
kvdbo.h
sfts.h
DESTINATION
${CMAKE_CURRENT_BINARY_DIR}/include/kvdb
)

set(LZ4_DIR "../third-party/lz4/lib")

include_directories(${LZ4_DIR})

add_library (kvdb
    kvassert.c
    kvblock.c
    kvdb.c
    kvprime.c
    kvtable.c
    kvdbo.cpp
    sfts.cpp
    kvunicode.c
    kvserialization.cpp
    ConvertUTF.c
    ${LZ4_DIR}/lz4.c
    ${LZ4_DIR}/lz4hc.c
    ${LZ4_DIR}/lz4frame.c
    ${LZ4_DIR}/xxhash.c
)
if(APPLE)
  target_link_libraries(kvdb "-framework CoreFoundation")
else()
  find_package(ICU REQUIRED COMPONENTS uc i18n)
  target_include_directories(kvdb PUBLIC ${ICU_INCLUDE_DIRS})
  target_link_libraries(kvdb ${ICU_LIBRARIES} pthread)
endif()

add_executable(kvdb_tests
    kvdb_tests.cpp
)
target_link_libraries(kvdb_tests kvdb)
add_test(NAME kvdb_tests COMMAND kvdb_tests)

add_executable(sfts_tests
    sfts_tests.cpp
)
target_link_libraries(sfts_tests kvdb)
add_test(NAME sfts_tests COMMAND sfts_tests)

add_executable(kvdbo_tests
    kvdbo_tests.cpp
)
target_link_libraries(kvdbo_tests kvdb)
add_test(NAME kvdbo_tests COMMAND kvdbo_tests)
