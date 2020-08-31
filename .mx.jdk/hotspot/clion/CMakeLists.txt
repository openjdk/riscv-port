cmake_minimum_required(VERSION 3.10)
project(jvmci_ce_11)
set(CMAKE_CONFIGURATION_TYPES "slowdebug;fastdebug;optimized;product" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 98)

set(REPO_ROOT ../../..)
set(BUILD_ROOT ${REPO_ROOT}/build)
set(HOTSPOT_SRC_ROOT ${REPO_ROOT}/src/hotspot)

if (UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif ()
if (APPLE)
    set(DARWIN TRUE)
endif()


if (CMAKE_BUILD_TYPE MATCHES product)
    add_definitions(-DPRODUCT)
endif()


if (CMAKE_BUILD_TYPE MATCHES .*debug)
    add_definitions(-DASSERT)
endif()

set(ADLC_SRC_DIR ${HOTSPOT_SRC_ROOT}/share/adlc)

set(ADLC_SOURCES
        ${ADLC_SRC_DIR}/adlparse.cpp
        ${ADLC_SRC_DIR}/archDesc.cpp
        ${ADLC_SRC_DIR}/arena.cpp
        ${ADLC_SRC_DIR}/dfa.cpp
        ${ADLC_SRC_DIR}/dict2.cpp
        ${ADLC_SRC_DIR}/filebuff.cpp
        ${ADLC_SRC_DIR}/forms.cpp
        ${ADLC_SRC_DIR}/formsopt.cpp
        ${ADLC_SRC_DIR}/formssel.cpp
        ${ADLC_SRC_DIR}/main.cpp
        ${ADLC_SRC_DIR}/output_c.cpp
        ${ADLC_SRC_DIR}/output_h.cpp
        ${HOTSPOT_SRC_ROOT}/share/opto/opcodes.cpp
        )

add_executable(adlc
        ${ADLC_SOURCES}
        )

target_include_directories(adlc PUBLIC
        ${ADLC_SRC_DIR}
        ${HOTSPOT_SRC_ROOT}/share
        )

add_definitions(-DDONT_USE_PRECOMPILED_HEADER)

set(SHARED_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/share)

if (${CMAKE_SYSTEM_PROCESSOR} STREQUAL "x86_64")
    set(HS_CPU x86)
    set(HS_MODEL 64)
    set(HS_ARCH amd64)
    set(ALT_HS_ARCH x86_64)
    add_definitions(-DAMD64)
endif()


add_definitions(-DTARGET_ARCH_${HS_CPU})
set(CPU_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/cpu/${HS_CPU})
file(GLOB CPU_JVM_SOURCES
        ${CPU_VM_SOURCES_DIR}/*${HS_CPU}.cpp
        ${CPU_VM_SOURCES_DIR}/*${HS_CPU}_${HS_MODEL}.cpp
        ${CPU_VM_SOURCES_DIR}/gc/shared/*${HS_CPU}.cpp
        ${CPU_VM_SOURCES_DIR}/gc/shared/g1/*${HS_CPU}.cpp
        ${CPU_VM_SOURCES_DIR}/gc/shared/z/*${HS_CPU}.cpp
        )

if (LINUX)
    add_definitions(-DLINUX -D_GNU_SOURCE)
    set(HS_OS linux)
elseif(DARWIN)
    add_definitions(-D_ALLBSD_SOURCE -D_GNU_SOURCE -D_XOPEN_SOURCE -D_DARWIN_C_SOURCE -DALLOW_OPERATOR_NEW_USAGE)
    set(HS_OS bsd)
endif()

set(OS_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os/${HS_OS})
file(GLOB OS_JVM_SOURCES
    ${OS_VM_SOURCES_DIR}/*.cpp
    )

if (${HS_OS} STREQUAL "windows")
    set(POSIX_VM_SOURCES_DIR )
    set(POSIX_VM_INCLUDE_DIR )
    set(POSIX_JVM_SOURCES )
else()
    set(POSIX_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os/posix)
    set(POSIX_VM_INCLUDE_DIR ${POSIX_VM_SOURCES_DIR}/include)
    file(GLOB POSIX_JVM_SOURCES
            ${POSIX_VM_SOURCES_DIR}/*.cpp
            )
endif()

set(HS_VARIANT compiler2)
set(JVM_VARIANT server)
set(JDK_VARIANT normal)

if (${HS_VARIANT} STREQUAL "compiler2")
    add_definitions(-DCOMPILER2)
    #assume tiered
    add_definitions(-DCOMPILER1)
endif()

#add_definitions(-DTARGET_OS_ARCH_${HS_OS}_${HS_CPU})
#add_definitions(-DTARGET_ARCH_MODEL_${HS_CPU}_${HS_MODEL})
add_definitions(-DTARGET_ARCH_${HS_CPU})
add_definitions(-DINCLUDE_SUFFIX_CPU=_${HS_CPU})
add_definitions(-DINCLUDE_SUFFIX_OS=_${HS_OS})
#add_definitions(-DTARGET_OS_FAMILY_${HS_OS})
add_definitions(-DHOTSPOT_LIB_ARCH=\"${HS_ARCH}\")
set(BUILD_DIR ${BUILD_ROOT}/${HS_OS}-${ALT_HS_ARCH}-${JDK_VARIANT}-${JVM_VARIANT}-${CMAKE_BUILD_TYPE})
set(GENERATED_DIR ${BUILD_DIR}/hotspot/variant-${JVM_VARIANT}/gensrc)
set(OS_CPU_VM_SOURCES_DIR ${HOTSPOT_SRC_ROOT}/os_cpu/${HS_OS}_${HS_CPU})
file(GLOB OS_CPU_JVM_SOURCES
        ${OS_CPU_VM_SOURCES_DIR}/*${HS_CPU}.cpp
        ${OS_CPU_VM_SOURCES_DIR}/*${HS_CPU}_${HS_MODEL}.cpp
        )
set(JAVA_BASE_SHARE_INCLUDE ${BUILD_DIR}/support/modules_include/java.base)
set(JAVA_BASE_MD_INCLUDE ${BUILD_DIR}/support/modules_include/java.base/${HS_OS})
set(JIMAGE_INCLUDE ${REPO_ROOT}/src/java.base/share/native/libjimage)


if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_definitions(-DTARGET_COMPILER_gcc)
    add_definitions(-DINCLUDE_SUFFIX_COMPILER=_gcc)
endif()
# This matches both Clang and AppleClang
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-DTARGET_COMPILER_gcc)
    add_definitions(-DINCLUDE_SUFFIX_COMPILER=_gcc)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-logical-op-parentheses -Wno-parentheses-equality -Wno-parentheses -Wno-switch -Wno-tautological-compare -Wno-delete-non-virtual-dtor -Wno-deprecated -Wno-format -Wno-dynamic-class-memaccess -Wno-empty-body -Wno-undefined-var-template -Wno-undefined-bool-conversion")
endif()

file(GLOB SHARED_JVM_SOURCES
        ${SHARED_VM_SOURCES_DIR}/aot/*.cpp
        ${SHARED_VM_SOURCES_DIR}/aot/*.c
        ${SHARED_VM_SOURCES_DIR}/aot/*.s
        ${SHARED_VM_SOURCES_DIR}/asm/*.cpp
        ${SHARED_VM_SOURCES_DIR}/asm/*.c
        ${SHARED_VM_SOURCES_DIR}/asm/*.s
        ${SHARED_VM_SOURCES_DIR}/c1/*.cpp
        ${SHARED_VM_SOURCES_DIR}/c1/*.c
        ${SHARED_VM_SOURCES_DIR}/c1/*.s
        ${SHARED_VM_SOURCES_DIR}/ci/*.cpp
        ${SHARED_VM_SOURCES_DIR}/ci/*.c
        ${SHARED_VM_SOURCES_DIR}/ci/*.s
        ${SHARED_VM_SOURCES_DIR}/classfile/*.cpp
        ${SHARED_VM_SOURCES_DIR}/classfile/*.c
        ${SHARED_VM_SOURCES_DIR}/classfile/*.s
        ${SHARED_VM_SOURCES_DIR}/code/*.cpp
        ${SHARED_VM_SOURCES_DIR}/code/*.c
        ${SHARED_VM_SOURCES_DIR}/code/*.s
        ${SHARED_VM_SOURCES_DIR}/compiler/*.cpp
        ${SHARED_VM_SOURCES_DIR}/compiler/*.c
        ${SHARED_VM_SOURCES_DIR}/compiler/*.s
        # missing: gc
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.cpp
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.c
        ${SHARED_VM_SOURCES_DIR}/interpreter/*.s
        ${SHARED_VM_SOURCES_DIR}/jfr/*.cpp
        ${SHARED_VM_SOURCES_DIR}/jfr/*.c
        ${SHARED_VM_SOURCES_DIR}/jfr/*.s
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.cpp
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.c
        ${SHARED_VM_SOURCES_DIR}/jvmci/*.s
        ${SHARED_VM_SOURCES_DIR}/libadt/*.cpp
        ${SHARED_VM_SOURCES_DIR}/libadt/*.c
        ${SHARED_VM_SOURCES_DIR}/libadt/*.s
        ${SHARED_VM_SOURCES_DIR}/logging/*.cpp
        ${SHARED_VM_SOURCES_DIR}/logging/*.c
        ${SHARED_VM_SOURCES_DIR}/logging/*.s
        ${SHARED_VM_SOURCES_DIR}/memory/*.cpp
        ${SHARED_VM_SOURCES_DIR}/memory/*.c
        ${SHARED_VM_SOURCES_DIR}/memory/*.s
        ${SHARED_VM_SOURCES_DIR}/oops/*.cpp
        ${SHARED_VM_SOURCES_DIR}/oops/*.c
        ${SHARED_VM_SOURCES_DIR}/oops/*.s
        ${SHARED_VM_SOURCES_DIR}/opto/*.cpp
        ${SHARED_VM_SOURCES_DIR}/opto/*.c
        ${SHARED_VM_SOURCES_DIR}/opto/*.s
        ${SHARED_VM_SOURCES_DIR}/prims/*.cpp
        ${SHARED_VM_SOURCES_DIR}/prims/*.c
        ${SHARED_VM_SOURCES_DIR}/prims/*.s
        ${SHARED_VM_SOURCES_DIR}/runtime/*.cpp
        ${SHARED_VM_SOURCES_DIR}/runtime/*.c
        ${SHARED_VM_SOURCES_DIR}/runtime/*.s
        ${SHARED_VM_SOURCES_DIR}/services/*.cpp
        ${SHARED_VM_SOURCES_DIR}/services/*.c
        ${SHARED_VM_SOURCES_DIR}/services/*.s
        ${SHARED_VM_SOURCES_DIR}/utilities/*.cpp
        ${SHARED_VM_SOURCES_DIR}/utilities/*.c
        ${SHARED_VM_SOURCES_DIR}/utilities/*.s
        ${GENERATED_DIR}/jvmtifiles/*.cpp
        ${GENERATED_DIR}/adfiles/*.cpp
        )

list(FILTER SHARED_JVM_SOURCES EXCLUDE REGEX ".*/jvmtiEnvStub.cpp$")

add_library(jvm
        ${SHARED_JVM_SOURCES}
        ${CPU_JVM_SOURCES}
        ${OS_JVM_SOURCES}
        ${OS_CPU_JVM_SOURCES}
        ${POSIX_JVM_SOURCES}
        )

target_include_directories(jvm PUBLIC
        ${SHARED_VM_SOURCES_DIR}
        ${SHARED_VM_SOURCES_DIR}/precompiled
        ${SHARED_VM_SOURCES_DIR}/include
        ${CPU_VM_SOURCES_DIR}
        ${OS_VM_SOURCES_DIR}
        ${POSIX_VM_SOURCES_DIR}
        ${POSIX_VM_INCLUDE_DIR}
        ${OS_CPU_VM_SOURCES_DIR}
        ${GENERATED_DIR}
        ${GENERATED_DIR}/adfiles
        ${JAVA_BASE_SHARE_INCLUDE}
        ${JAVA_BASE_MD_INCLUDE}
        ${JIMAGE_INCLUDE}
        )
