#!/bin/bash

cd client/Android/FreeRDPCore/jni/generated/
javah -classpath ../../bin/classes:@ANDROID_SDK@/platforms/android-8/android.jar -jni com.freerdp.freerdpcore.services.LibFreeRDP 
rm com_freerdp_freerdpcore_services_LibFreeRDP_EventListener.h \
   com_freerdp_freerdpcore_services_LibFreeRDP_UIEventListener.h
