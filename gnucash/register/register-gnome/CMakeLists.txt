include(CheckSymbolExists)

set (register_gnome_SOURCES
  combocell-gnome.cpp
  completioncell-gnome.cpp
  datecell-gnome.cpp
  formulacell-gnome.cpp
  gnucash-color.cpp
  gnucash-cursor.cpp
  gnucash-date-picker.cpp
  gnucash-header.cpp
  gnucash-item-edit.cpp
  gnucash-item-list.cpp
  gnucash-register.cpp
  gnucash-sheet-private.cpp
  gnucash-sheet.cpp
  gnucash-style.cpp
  pricecell-gnome.cpp
  quickfillcell-gnome.cpp
  table-gnome.cpp
)

set (register_gnome_noinst_HEADERS
  formulacell-gnome.h
  gnucash-color.h
  gnucash-cursor.h
  gnucash-date-picker.h
  gnucash-header.h
  gnucash-item-edit.h
  gnucash-item-list.h
  gnucash-register.h
  gnucash-sheet.h
  gnucash-sheetP.h
  gnucash-style.h
  pricecell-gnome.h
  quickfillcell-gnome.h
  table-gnome.h
)

# Add dependency on config.h
set_source_files_properties (${register_gnome_SOURCES} PROPERTIES OBJECT_DEPENDS ${CONFIG_H})

add_library (gnc-register-gnome ${register_gnome_SOURCES} ${register_gnome_noinst_HEADERS})

if (MINGW)
  # ensure no C compilation
  set_target_properties(gnc-register-gnome PROPERTIES
        CMAKE_C_COMPILER_LAUNCHER "false"
  )
  # remove -DWINVER=0x0500 -D_EMULATE_GLIBC=0 and add C++ options from non-MINGW compile
  # By default target_compile_options() adds its flags AFTER CMAKE_CXX_FLAGS
  set(NEW_CXX_COMPILE_FLAGS "-Werror" "-Wall" "-Wmissing-declarations"
    "-UWINVER" "-U_EMULATE_GLIBC")
  # set target compile options
  target_compile_options(gnc-register-gnome PRIVATE ${NEW_CXX_COMPILE_FLAGS}
  )
endif()

target_link_libraries(gnc-register-gnome gnc-register-core PkgConfig::GTK3)

target_compile_definitions(gnc-register-gnome PRIVATE -DG_LOG_DOMAIN=\"gnc.register.gnome\")

target_include_directories(gnc-register-gnome
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/gnucash/register/ledger-core
)

if (APPLE)
  set_target_properties (gnc-register-gnome PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/gnucash")
endif()

if (COVERAGE)
  add_coverage_target(gnc-register-gnome)
endif()

install(TARGETS gnc-register-gnome
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gnucash
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/gnucash
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# No headers to install

set_local_dist(register_gnome_DIST_local CMakeLists.txt ${register_gnome_SOURCES} ${register_gnome_noinst_HEADERS})

set(register_gnome_DIST ${register_gnome_DIST_local} ${test_register_gnome_DIST} PARENT_SCOPE)
