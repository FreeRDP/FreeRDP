include(CheckAndSetFlag)

option(EXPORT_ALL_SYMBOLS "Export all symbols form library" OFF)

if (EXPORT_ALL_SYMBOLS)
  add_compile_definitions(EXPORT_ALL_SYMBOLS)
else()
  message(STATUS "${} default symbol visibility: hidden")

  CheckAndSetFlag(-fvisibility=hidden)
endif()


