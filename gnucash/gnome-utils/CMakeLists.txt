# Note that gnucash/gnome-utils CANNOT depend on gnucash/gnome!

add_subdirectory(test)

# Command to generate the swig-gnome-utils.c wrapper file
gnc_add_swig_guile_command (swig-gnome-utils-c
    SWIG_GNOME_UTILS_C swig-gnome-utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gnome-utils.i ""
)

add_dependencies(swig-gnome-utils-c gnc-gnome-utils)
set (WARNINGS_SCHEMA ${DATADIR_BUILD}/glib-2.0/schemas/org.gnucash.GnuCash.warnings.gschema.xml)
set (GNC_WARNINGS_C ${CMAKE_CURRENT_BINARY_DIR}/gnc-warnings.c)
set (GNC_WARNINGS_H ${CMAKE_CURRENT_BINARY_DIR}/gnc-warnings.h)

add_custom_command(
  OUTPUT ${GNC_WARNINGS_C}
  DEPENDS ${WARNINGS_SCHEMA} make-gnc-warnings-c.xsl
  COMMAND
    ${LIBXSLT_XSLTPROC_EXECUTABLE} -o ${GNC_WARNINGS_C} ${CMAKE_CURRENT_SOURCE_DIR}/make-gnc-warnings-c.xsl ${WARNINGS_SCHEMA}
)
add_custom_command(
  OUTPUT ${GNC_WARNINGS_H}
  DEPENDS ${WARNINGS_SCHEMA} make-gnc-warnings-h.xsl
  COMMAND
    ${LIBXSLT_XSLTPROC_EXECUTABLE} -o ${GNC_WARNINGS_H} ${CMAKE_CURRENT_SOURCE_DIR}/make-gnc-warnings-h.xsl ${WARNINGS_SCHEMA}
)

set (gnome_utils_SOURCES
  account-quickfill.cpp
  assistant-xml-encoding.cpp
  cursors.cpp
  dialog-account.cpp
  dialog-doclink-utils.cpp
  dialog-book-close.cpp
  dialog-commodity.cpp
  dialog-dup-trans.cpp
  dialog-file-access.cpp
  dialog-object-references.cpp
  dialog-options.cpp
  dialog-preferences.cpp
  dialog-query-view.cpp
  dialog-reset-warnings.cpp
  dialog-tax-table.cpp
  dialog-totd.cpp
  dialog-transfer.cpp
  dialog-userpass.cpp
  dialog-utils.cpp
  gnc-account-sel.cpp
  gnc-amount-edit.cpp
  gnc-autosave.cpp
  gnc-cell-renderer-text-flag.cpp
  gnc-cell-renderer-text-view.cpp
  gnc-cell-view.cpp
  gnc-commodity-edit.cpp
  gnc-component-manager.cpp
  gnc-currency-edit.cpp
  gnc-date-edit.cpp
  gnc-date-format.cpp
  gnc-dense-cal.cpp
  gnc-dense-cal-model.cpp
  gnc-dense-cal-store.cpp
  gnc-embedded-window.cpp
  gnc-file.cpp
  gnc-frequency.cpp
  gnc-recurrence.cpp
  gnc-general-select.cpp
  gnc-gnome-utils.cpp
  gnc-gobject-utils.cpp
  gnc-gtk-utils.cpp
  gnc-gui-query.cpp
  gnc-icons.cpp
  gnc-keyring.cpp
  gnc-main-window.cpp
  gnc-menu-extensions.cpp
  gnc-option-gtk-ui.cpp
  gnc-plugin-file-history.cpp
  gnc-plugin-manager.cpp
  gnc-plugin-menu-additions.cpp
  gnc-plugin-page.cpp
  gnc-plugin.cpp
  gnc-period-select.cpp
  gnc-query-view.cpp
  gnc-report-combo.cpp
  gnc-splash.cpp
  gnc-sx-instance-dense-cal-adapter.cpp
  gnc-sx-list-tree-model-adapter.cpp
  gnc-tree-model.cpp
  gnc-tree-model-account-types.cpp
  gnc-tree-model-account.cpp
  gnc-tree-model-budget.cpp
  gnc-tree-model-owner.cpp
  gnc-tree-model-commodity.cpp
  gnc-tree-model-price.cpp
  gnc-tree-view-account.cpp
  gnc-tree-view-commodity.cpp
  gnc-tree-view-owner.cpp
  gnc-tree-view-price.cpp
  gnc-tree-view-sx-list.cpp
  gnc-tree-view.cpp
  gnc-window.cpp
  misc-gnome-utils.cpp
  tree-view-utils.cpp
  search-param.cpp
  print-session.cpp
  window-main-summarybar.cpp
)

set(gnome_utils_noinst_HEADERS
  dialog-tax-table.h
  dialog-options.hpp
  gnc-autosave.hpp
  gnc-gobject-utils.h
  gnc-gtk-utils.h
  gnc-option-gtk-ui.hpp
  search-param.h
)

set (gnome_utils_HEADERS
  account-quickfill.h
  assistant-xml-encoding.hpp
  dialog-account.h
  dialog-doclink-utils.h
  dialog-book-close.h
  dialog-commodity.h
  dialog-dup-trans.h
  dialog-file-access.h
  dialog-preferences.h
  dialog-object-references.hpp
  dialog-query-view.h
  dialog-reset-warnings.hpp
  dialog-totd.h
  dialog-transfer.h
  dialog-utils.h
  gnc-account-sel.h
  gnc-amount-edit.h
  gnc-cell-renderer-text-flag.h
  gnc-cell-renderer-text-view.hpp
  gnc-cell-view.hpp
  gnc-commodity-edit.h
  gnc-component-manager.h
  gnc-currency-edit.h
  gnc-date-edit.h
  gnc-date-format.h
  gnc-dense-cal.h
  gnc-dense-cal-model.h
  gnc-dense-cal-store.h
  gnc-embedded-window.h
  gnc-file.h
  gnc-frequency.h
  gnc-recurrence.h
  gnc-general-select.h
  gnc-gnome-utils.h
  gnc-gui-query.h
  gnc-icons.h
  gnc-keyring.hpp
  gnc-main-window.h
  gnc-menu-extensions.h
  gnc-plugin-file-history.h
  gnc-plugin-manager.h
  gnc-plugin-menu-additions.h
  gnc-plugin-page.h
  gnc-plugin.h
  gnc-period-select.hpp
  gnc-query-view.h
  gnc-report-combo.h
  gnc-splash.hpp
  gnc-sx-instance-dense-cal-adapter.hpp
  gnc-sx-list-tree-model-adapter.hpp
  gnc-tree-model.hpp
  gnc-tree-model-account-types.h
  gnc-tree-model-account.hpp
  gnc-tree-model-budget.h
  gnc-tree-model-owner.hpp
  gnc-tree-model-commodity.hpp
  gnc-tree-model-price.hpp
  gnc-tree-view-account.h
  gnc-tree-view-commodity.hpp
  gnc-tree-view-owner.hpp
  gnc-tree-view-price.hpp
  gnc-tree-view-sx-list.hpp
  gnc-tree-view.h
  gnc-ui.h
  gnc-window.h
  misc-gnome-utils.h
  tree-view-utils.h
  print-session.h
  window-main-summarybar.hpp
  )



add_library (gnc-gnome-utils
  ${gnome_utils_SOURCES}
  ${gnome_utils_HEADERS}
  ${GNC_WARNINGS_C} ${GNC_WARNINGS_H}
  ${SWIG_GNOME_UTILS_C}
  ${gnome_utils_noinst_HEADERS}
)

target_link_libraries(gnc-gnome-utils
    gnc-app-utils
    gnc-engine
    gnc-expressions
    gnc-locale-tax
    gnc-backend-xml-utils
    gnucash-guile
    PkgConfig::GTK3
    ${CMAKE_DL_LIBS}
    ${LIBSECRET_LDFLAGS}
    ${GTK_MAC_LDFLAGS})

target_compile_definitions(gnc-gnome-utils PUBLIC ${GTK_MAC_CFLAGS_OTHER}
  PRIVATE -DG_LOG_DOMAIN=\"gnc.gui\")

if (BUILDING_FROM_VCS)
  target_compile_definitions(gnc-gnome-utils PRIVATE -DGNC_VCS=\"git\")
endif()

if (APPLE)
  target_compile_options(gnc-gnome-utils PRIVATE ${OSX_EXTRA_COMPILE_FLAGS})
  target_link_libraries(gnc-gnome-utils ${OSX_EXTRA_LIBRARIES})
endif()

if (COVERAGE)
  add_coverage_target(gnc-gnome-utils)
endif()

target_include_directories(gnc-gnome-utils
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
      ${GTK_MAC_INCLUDE_DIRS}
      ${LIBSECRET_INCLUDE_DIRS}
      ${CMAKE_CURRENT_BINARY_DIR}
)


if (APPLE)
  set_target_properties (gnc-gnome-utils PROPERTIES INSTALL_NAME_DIR "${CMAKE_INSTALL_FULL_LIBDIR}/gnucash")
endif()

install(TARGETS gnc-gnome-utils
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/gnucash
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/gnucash
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${gnome_utils_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/gnucash)


# Scheme

set(GUILE_DEPENDS      gnc-gnome-utils scm-engine scm-app-utils)

gnc_add_scheme_targets(scm-gnome-utils-1
    SOURCES gnc-menu-extensions.scm
    OUTPUT_DIR "gnucash/gnome-utils"
    DEPENDS "${GUILE_DEPENDS}"
    MAKE_LINKS)

gnc_add_scheme_targets(scm-gnome-utils-2
    SOURCES gnome-utils.scm
    OUTPUT_DIR gnucash
    DEPENDS "scm-gnome-utils-1"
    MAKE_LINKS)

add_custom_target(scm-gnome-utils ALL DEPENDS scm-gnome-utils-2 scm-gnome-utils-1)

set_local_dist(gnome_utils_DIST_local CMakeLists.txt ${gnome_utils_SOURCES} ${gnome_utils_HEADERS}
        ${gnome_utils_noinst_HEADERS} gnome-utils.scm gnome-utils.i gnc-menu-extensions.scm
        make-gnc-warnings-c.xsl make-gnc-warnings-h.xsl)
set(gnome_utils_DIST ${gnome_utils_DIST_local} ${test_gnome_utils_DIST} PARENT_SCOPE)
