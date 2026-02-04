# - macro_add_source_file_compile_flags(<_target> "flags...")

# Copyright (c) 2006, Oswald Buddenhagen, <ossi@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


macro (macro_add_source_file_compile_flags _sourcefile _additionalflags)

   get_source_file_property (_flags ${_sourcefile} COMPILE_FLAGS)
   if (_flags)
      set(_flags "${_flags} ${_additionalflags}")
   else()
      set(_flags "${_additionalflags}")
   endif()
   set_source_files_properties (${_sourcefile} PROPERTIES COMPILE_FLAGS "${_flags}")

endmacro (macro_add_source_file_compile_flags)

macro (save_orig_compile_flags)
   if (NOT DEFINED ORIGINAL_CMAKE_CXX_FLAGS)
      set (ORIGINAL_CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
      set (ORIGINAL_CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
   endif()
endmacro(save_orig_compile_flags)

macro (unix_init_compile_flags)
   save_orig_compile_flags()

   set(CMAKE_C_FLAGS "-Werror -Wall -Wmissing-prototypes -Wmissing-declarations ${ORIGINAL_CMAKE_C_FLAGS}")
   set(CMAKE_CXX_FLAGS "-Werror -Wall -Wmissing-declarations ${ORIGINAL_CMAKE_CXX_FLAGS}")
   set(CMAKE_C_FLAGS_RELEASE "-O3 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 ${CMAKE_C_FLAGS}")
endmacro (unix_init_compile_flags)

macro (mingw_init_compile_flags)
   save_orig_compile_flags()

   set(CMAKE_C_FLAGS "-Werror -Wall -Wmissing-prototypes -Wmissing-declarations ${ORIGINAL_CMAKE_C_FLAGS}")
   # are these still needed?
   # MINVER=0x0500 is WindowsXP - do we really support GnuCash building on WindowsXP?
   # _EMULATE_GLIBC=0 I think is to get "WIN32" definition defined, can we use _WIN32 or just add it to CFLAGS?
   set(CMAKE_CXX_FLAGS "-DWINVER=0x0500 -D_EMULATE_GLIBC=0 ${ORIGINAL_CMAKE_CXX_FLAGS}") # Workaround for bug in gtest on mingw, see https://github.com/google/googletest/issues/893 and https://github.com/google/googletest/issues/920
   # NOTYET: set( CMAKE_CXX_FLAGS "-Werror -Wall -Wmissing-declarations ${CMAKE_CXX_FLAGS}")
endmacro (mingw_init_compile_flags)

# for projects/modules that have been improved
macro (mingw_promote_compile_flags)
   set(CMAKE_CXX_FLAGS "-Werror -Wall -Wmissing-declarations ${ORIGINAL_CMAKE_CXX_FLAGS}")
endmacro (mingw_promote_compile_flags)

macro (apple_init_compile_flags)
   save_orig_compile_flags()

   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-gnu-folding-constant")
endmacro(apple_init_compile_flags)

