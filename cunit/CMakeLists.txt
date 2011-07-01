# cunit

include_directories(.)
include_directories(../include)
include_directories(../libfreerdp-gdi)

add_executable(test_freerdp
	test_color.c
	test_color.h
	test_libgdi.c
	test_libgdi.h
	test_freerdp.c
	test_freerdp.h)

target_link_libraries(test_freerdp cunit)

target_link_libraries(test_freerdp freerdp-gdi)
target_link_libraries(test_freerdp freerdp-asn1)
target_link_libraries(test_freerdp freerdp-utils)

