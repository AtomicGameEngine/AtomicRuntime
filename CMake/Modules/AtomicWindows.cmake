

include(AtomicDesktop)

#set (CMAKE_DEBUG_POSTFIX _d)

set (D3DCOMPILER_47_DLL ${CMAKE_SOURCE_DIR}/Build/Windows/Binaries/x64/D3DCompiler_47.dll)

add_definitions(-DATOMIC_PLATFORM_WINDOWS -D_CRT_SECURE_NO_WARNINGS -DATOMIC_TBUI)

list (APPEND ATOMIC_LINK_LIBRARIES MojoShader user32 gdi32 winmm imm32 ole32 oleaut32 version uuid d3d9 d3dcompiler Ws2_32)

# compile with static runtime
set(CompilerFlags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE)

foreach(CompilerFlag ${CompilerFlags})
    string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()
