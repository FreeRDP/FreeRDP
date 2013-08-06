# Try to find DocBook XSL stylesheet
# Once done, it will define:
#
# DOCBOOKXSL_FOUND - system has the required DocBook XML DTDs
# DOCBOOKXSL_DIR - the directory containing the stylesheets
# used to process DocBook XML

# Copyright (c) 2010, Luigi Toscano, <luigi.toscano@tiscali.it>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

set (STYLESHEET_PATH_LIST
    share/xml/docbook/stylesheet/docbook-xsl
    share/xml/docbook/xsl-stylesheets
    share/sgml/docbook/xsl-stylesheets
    share/xml/docbook/stylesheet/nwalsh/current
    share/xml/docbook/stylesheet/nwalsh
    share/xsl/docbook
    share/xsl/docbook-xsl
)

find_path (DOCBOOKXSL_DIR lib/lib.xsl
   PATHS ${CMAKE_SYSTEM_PREFIX_PATH}
   PATH_SUFFIXES ${STYLESHEET_PATH_LIST}
)

if (NOT DOCBOOKXSL_DIR)
   # hacks for systems that put the version in the stylesheet dirs
   set (STYLESHEET_PATH_LIST)
   foreach (STYLESHEET_PREFIX_ITER ${CMAKE_SYSTEM_PREFIX_PATH})
      file(GLOB STYLESHEET_SUFFIX_ITER RELATIVE ${STYLESHEET_PREFIX_ITER}
           ${STYLESHEET_PREFIX_ITER}/share/xml/docbook/xsl-stylesheets-*
      )
      if (STYLESHEET_SUFFIX_ITER)
         list (APPEND STYLESHEET_PATH_LIST ${STYLESHEET_SUFFIX_ITER})
      endif ()
   endforeach ()

   find_path (DOCBOOKXSL_DIR VERSION
      PATHS ${CMAKE_SYSTEM_PREFIX_PATH}
      PATH_SUFFIXES ${STYLESHEET_PATH_LIST}
   )
endif (NOT DOCBOOKXSL_DIR)


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args (DocBookXSL
                                   "Could NOT find DocBook XSL stylesheets"
                                   DOCBOOKXSL_DIR)

mark_as_advanced (DOCBOOKXSL_DIR)
