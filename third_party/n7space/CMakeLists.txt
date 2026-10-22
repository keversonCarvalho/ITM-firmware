# N7 Space lely-core v3.5.0, peeled commit recorded in UPSTREAM.json.
# Embedded feature selection mirrors --enable-ecss-compliance in configure.ac.
set(n7_sources)
foreach(file stdatomic string strings)
    list(APPEND n7_sources lib/compat/${file}.c)
endforeach()
foreach(file bits cmp diag dllist endian error memory mempool membuf pheap rbtree time ustring)
    list(APPEND n7_sources lib/util/${file}.c)
endforeach()
foreach(file buf msg net)
    list(APPEND n7_sources lib/can/${file}.c)
endforeach()
foreach(file crc dev emcy nmt nmt_hb nmt_rdn nmt_srv obj pdo rpdo sdo ssdo sync tpdo type val)
    list(APPEND n7_sources lib/co/${file}.c)
endforeach()
add_library(itm_n7space STATIC ${n7_sources})
target_include_directories(itm_n7space SYSTEM PUBLIC include)
target_compile_features(itm_n7space PUBLIC c_std_11)
target_compile_definitions(itm_n7space PUBLIC
    LELY_NO_HOSTED=1 LELY_NO_RT=1 LELY_NO_THREADS=1 LELY_NO_ERRNO=1
    LELY_NO_MALLOC=1 LELY_NO_STDIO=1 LELY_NO_CXX=1 LELY_NO_DAEMON=1
    LELY_NO_DIAG=1 LELY_NO_CANFD=1 LELY_NO_CO_DCF=1
    LELY_NO_CO_OBJ_FILE=1 LELY_NO_CO_OBJ_NAME=1 LELY_NO_CO_SDEV=1
    LELY_NO_CO_TIME=1 LELY_NO_CO_LSS=1 LELY_NO_CO_WTM=1 LELY_NO_CO_NG=1
    LELY_NO_CO_NMT_BOOT=1 LELY_NO_CO_NMT_CFG=1 LELY_NO_CO_GW=1
    LELY_NO_CO_GW_TXT=1 LELY_NO_CO_CSDO=1 LELY_NO_CO_MASTER=1
    LELY_NO_CO_ECSS_REDUNDANCY=0 CO_NMT_MAX_NHB=1)
if(NOT MSVC)
    target_compile_options(itm_n7space PRIVATE -ffunction-sections -fdata-sections)
endif()
