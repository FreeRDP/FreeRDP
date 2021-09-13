                
find_path(X264_INCLUDE_DIR x264.h)
                                
find_library(X264_LIBRARY x264) 
                                
find_package_handle_standard_args(X264 DEFAULT_MSG X264_INCLUDE_DIR X264_LIBRARY)
                                
if(X264_FOUND)                  
        set(X264_LIBRARIES ${X264_LIBRARY})
        set(X264_INCLUDE_DIRS ${X264_INCLUDE_DIR})
endif()         
                        
mark_as_advanced(X264_INCLUDE_DIR X264_LIBRARY)

