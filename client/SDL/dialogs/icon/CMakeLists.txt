add_library(icon OBJECT
	FreeRDP_Icon.hpp
	FreeRDP_Icon.cpp
)
set_property(TARGET font PROPERTY POSITION_INDEPENDENT_CODE ON)
