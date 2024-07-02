if(CMAKE_CROSSCOMPILING)
   find_package(GenerateArgumentDocbook)
else()
    add_executable(generate_argument_docbook
        generate_argument_docbook.c
    )
   export(TARGETS generate_argument_docbook FILE
          "${CMAKE_BINARY_DIR}/GenerateArgumentDocbookConfig.cmake")
endif()
