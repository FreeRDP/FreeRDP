#!/bin/bash

cd client/Android/jni/generated/
javah -classpath ../../bin/classes/ -jni com.freerdp.afreerdp.services.LibFreeRDP 
rm com_freerdp_afreerdp_services_LibFreeRDP_EventListener.h \
   com_freerdp_afreerdp_services_LibFreeRDP_UIEventListener.h
