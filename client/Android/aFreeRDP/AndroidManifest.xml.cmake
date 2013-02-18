<?xml version="1.0" encoding="UTF-8"?>

<manifest xmlns:android="http://schemas.android.com/apk/res/android"
	android:installLocation="auto"
	package="com.freerdp.afreerdp"
	android:versionCode="1"
	android:versionName="@FREERDP_VERSION_FULL@" >

	<uses-sdk android:targetSdkVersion="8" android:minSdkVersion="8"/>
  <supports-screens android:anyDensity="true" android:smallScreens="true" android:normalScreens="true" android:largeScreens="true" />
			
	<application android:name="com.freerdp.afreerdp.application.GlobalApp"
		android:label="aFreeRDP"
		android:icon="@drawable/icon_launcher_freerdp" >

			<!-- Main activity -->
			<activity android:name="com.freerdp.freerdpcore.presentation.HomeActivity"
				android:label="@string/app_title"
				android:theme="@style/Theme.Main"
				android:alwaysRetainTaskState="true"
				>
				<intent-filter android:label="@string/app_title">
					<action android:name="android.intent.action.MAIN"/>
					<category android:name="android.intent.category.LAUNCHER"/>
				</intent-filter>
			</activity>

	</application>
	
</manifest>
