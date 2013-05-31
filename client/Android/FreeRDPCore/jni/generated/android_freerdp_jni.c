/*
   FreeRDP: A Remote Desktop Protocol client.
   Android JNI Bindings and Native Code

   Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "android_freerdp.h"
#include "android_freerdp_jni.h"

JNIEXPORT jint JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1new(JNIEnv *env, jclass cls)
{
	return jni_freerdp_new(env, cls);
}

JNIEXPORT void JNICALL JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1free(JNIEnv *env, jclass cls, jint instance)
{
	jni_freerdp_free(env, cls, instance);
}

JNIEXPORT jboolean JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1connect(JNIEnv *env, jclass cls, jint instance)
{
	return jni_freerdp_connect(env, cls, instance);
}

JNIEXPORT jboolean JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1disconnect(JNIEnv *env, jclass cls, jint instance)
{
	return jni_freerdp_disconnect(env, cls, instance);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1cancel_1connection(JNIEnv *env, jclass cls, jint instance)
{
	jni_freerdp_cancel_connection(env, cls, instance);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1connection_1info(JNIEnv *env, jclass cls, jint instance,
	jstring jhostname, jstring jusername, jstring jpassword, jstring jdomain, jint width, jint height, jint color_depth, jint port,
	jboolean console, jint security, jstring certname)
{
	jni_freerdp_set_connection_info(env, cls, instance, jhostname, jusername, jpassword, jdomain,
			width, height, color_depth, port, console, security, certname);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1advanced_1settings(JNIEnv *env, jclass cls, jint instance, jstring remote_program, jstring work_dir)
{
	jni_freerdp_set_advanced_settings(env, cls, instance, remote_program, work_dir);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1data_1directory(JNIEnv *env, jclass cls, jint instance, jstring directory)
{
	jni_freerdp_set_data_directory(env, cls, instance, directory);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1performance_1flags(
	JNIEnv *env, jclass cls, jint instance, jboolean remotefx, jboolean disableWallpaper, jboolean disableFullWindowDrag,
	jboolean disableMenuAnimations, jboolean disableTheming, jboolean enableFontSmoothing, jboolean enableDesktopComposition)
{
	jni_freerdp_set_performance_flags(env, cls, instance, remotefx, disableWallpaper, disableFullWindowDrag, disableMenuAnimations, disableTheming, enableFontSmoothing, enableDesktopComposition);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1clipboard_1redirection
  (JNIEnv *env, jclass cls, jint inst, jboolean enable)
{
	jni_freerdp_set_clipboard_redirection(env, cls, inst, enable);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1drive_1redirection
  (JNIEnv *env, jclass cls, jint inst, jstring path)
{
	jni_freerdp_set_drive_redirection(env, cls, inst, path);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1set_1gateway_1info
  (JNIEnv *env, jclass cls, jint inst, jstring hostname, jint port, jstring username, jstring password, jstring domain)
{
	jni_freerdp_set_gateway_info(env, cls, inst, hostname, port, username, password, domain);
}

JNIEXPORT jboolean JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1update_1graphics(
		JNIEnv *env, jclass cls, jint instance, jobject bitmap, jint x, jint y, jint width, jint height)
{
	return jni_freerdp_update_graphics(env, cls, instance, bitmap, x, y, width, height);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1cursor_1event(
	JNIEnv *env, jclass cls, jint instance, jint x, jint y, jint flags)
{
	jni_freerdp_send_cursor_event(env, cls, instance, x, y, flags);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1key_1event(
	JNIEnv *env, jclass cls, jint instance, jint keycode, jboolean down)
{
	jni_freerdp_send_key_event(env, cls, instance, keycode, down);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1unicodekey_1event
  (JNIEnv *env, jclass cls, jint instance, jint keycode)
{
	jni_freerdp_send_unicodekey_event(env, cls, instance, keycode);
}

JNIEXPORT void JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1send_1clipboard_1data
  (JNIEnv *env, jclass cls, jint instance, jstring data)
{	
	jni_freerdp_send_clipboard_data(env, cls, instance, data);
}

JNIEXPORT jstring JNICALL Java_com_freerdp_freerdpcore_services_LibFreeRDP_freerdp_1get_1version(JNIEnv *env, jclass cls)
{
	return jni_freerdp_get_version(env, cls);
}

