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

			<!-- Session request handler activity - used for search and internally to start sessions -->
			<!-- This should actually be defined in FreeRDPCore lib but Android manifest merging will -->
			<!-- append the libs manifest to the apps manifest and therefore aliasing is not possible -->
			<activity android:name="com.freerdp.freerdpcore.services.SessionRequestHandlerActivity"
				android:theme="@android:style/Theme.NoDisplay"
				android:noHistory="true"
				android:excludeFromRecents="true">
			</activity>

	       		<activity-alias android:name=".services.SessionRequestHandlerActivity"
		            android:targetActivity="com.freerdp.freerdpcore.services.SessionRequestHandlerActivity">	
				<intent-filter>
					<action android:name="android.intent.action.SEARCH" />
				</intent-filter>
				<meta-data android:name="android.app.searchable" 
					android:resource="@xml/searchable" />
		        </activity-alias>				

			<provider android:name="com.freerdp.freerdpcore.services.FreeRDPSuggestionProvider"
				android:authorities="com.freerdp.afreerdp.services.freerdpsuggestionprovider"
				>
			</provider>

	</application>
	
</manifest>
