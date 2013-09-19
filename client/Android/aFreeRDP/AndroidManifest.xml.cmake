<?xml version="1.0" encoding="UTF-8"?>

<manifest xmlns:android="http://schemas.android.com/apk/res/android"
	android:installLocation="auto"
	package="com.freerdp.afreerdp"
	android:versionCode="3"
	android:versionName="@GIT_REVISION@" >

	<uses-sdk android:targetSdkVersion="@ANDROID_APP_TARGET_SDK@" android:minSdkVersion="@ANDROID_APP_MIN_SDK@"/>
	<supports-screens android:anyDensity="true" android:smallScreens="true" android:normalScreens="true" android:largeScreens="true" android:xlargeScreens="true" />
			
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
				<intent-filter>
					<action android:name="android.intent.action.VIEW" />
					<category android:name="android.intent.category.DEFAULT" />              
					<category android:name="android.intent.category.BROWSABLE" />
					<data android:scheme="http" android:host="*" android:pathPattern=".*\\.rdp" />
					<data android:scheme="file" android:host="*" android:pathPattern=".*\\.rdp" />        					
					<data android:mimeType="*/*" />
				</intent-filter>
			</activity>

			<!-- Session request handler activity - used for search and internally to start sessions -->
			<!-- This should actually be defined in FreeRDPCore lib but Android manifest merging will -->
			<!-- append the libs manifest to the apps manifest and therefore aliasing is not possible -->
			<activity android:name="com.freerdp.freerdpcore.services.SessionRequestHandlerActivity"
				android:theme="@android:style/Theme.NoDisplay"
				android:noHistory="true"
				android:excludeFromRecents="true">
				<intent-filter>
				    <action android:name="android.intent.action.MAIN"/>
				</intent-filter>
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

