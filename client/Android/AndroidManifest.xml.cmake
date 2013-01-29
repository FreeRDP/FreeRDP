<?xml version="1.0" encoding="UTF-8"?>

<manifest xmlns:android="http://schemas.android.com/apk/res/android"
	android:installLocation="auto"
	package="com.freerdp.afreerdp"
	android:versionCode="1"
	android:versionName="@FREERDP_VERSION_FULL@" >

	<uses-sdk android:targetSdkVersion="8" android:minSdkVersion="8"/>
	<uses-permission android:name="android.permission.INTERNET"/>
	<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE"/>
	<uses-permission android:name="com.android.vending.BILLING" />
    <supports-screens android:anyDensity="true" android:smallScreens="true" android:normalScreens="true" android:largeScreens="true" />
			
	<application android:name=".application.GlobalApp"
		android:label="aFreeRDP"
		android:icon="@drawable/icon_launcher_freerdp" >

			<!-- Main activity -->
			<activity android:name=".presentation.HomeActivity"
				android:label="@string/app_title"
				android:theme="@style/Theme.Main"
				android:alwaysRetainTaskState="true"
				>
				<intent-filter android:label="@string/app_title">
					<action android:name="android.intent.action.MAIN"/>
					<category android:name="android.intent.category.LAUNCHER"/>
				</intent-filter>
			</activity>

			<!-- Acra crash report dialog activity -->
			<activity android:name="org.acra.CrashReportDialog"
                        android:theme="@android:style/Theme.Dialog" android:launchMode="singleInstance"
                        android:excludeFromRecents="true" android:finishOnTaskLaunch="true" />
			
			<!-- Session request handler activity - used for search and internally to start sessions -->
			<activity android:name=".services.SessionRequestHandlerActivity"
				android:theme="@android:style/Theme.NoDisplay"
				android:noHistory="true"
				android:excludeFromRecents="true">
				<intent-filter>
					<action android:name="android.intent.action.SEARCH" />
				</intent-filter>
				<meta-data android:name="android.app.searchable" 
					android:resource="@xml/searchable" />
			</activity>

			<!-- Activity to create shortcuts -->
	        <activity android:name=".presentation.ShortcutsActivity"
	  				  android:theme="@style/Theme.Main"
	                  android:label="@string/title_create_shortcut">
	            <intent-filter>
	                <action android:name="android.intent.action.MAIN" />
	                <category android:name="android.intent.category.DEFAULT" />
	            </intent-filter>
	
	        </activity>
			
	        <!-- It is recommended that you use an activity-alias to provide the "CREATE_SHORTCUT" -->
	        <!-- intent-filter.  This gives you a way to set the text (and optionally the -->
	        <!-- icon) that will be seen in the launcher's create-shortcut user interface. -->	
	        <activity-alias android:name=".presentation.CreateShortcuts"
	            android:targetActivity=".presentation.ShortcutsActivity"
	            android:label="@string/title_create_shortcut">	
	            <!--  This intent-filter allows your shortcuts to be created in the launcher. -->
	            <intent-filter>
	                <action android:name="android.intent.action.CREATE_SHORTCUT" />
	                <category android:name="android.intent.category.DEFAULT" />
	            </intent-filter>	
	        </activity-alias>					
						
			<activity android:name=".presentation.BookmarkActivity"
				android:label="@string/title_bookmark_settings"
				android:theme="@style/Theme.Settings">
			</activity>
			<activity android:name=".presentation.ApplicationSettingsActivity"
				android:label="@string/title_application_settings"
				android:theme="@style/Theme.Settings"
				android:windowSoftInputMode="stateHidden">
			</activity>
			<activity android:name=".presentation.SessionActivity"
				android:theme="@android:style/Theme.Black.NoTitleBar"
				android:configChanges="orientation|keyboardHidden|keyboard"
				android:windowSoftInputMode="adjustResize">
			</activity>
			<activity android:name=".presentation.AboutActivity"
				android:label="@string/title_about"
				android:theme="@style/Theme.Main">
			</activity>
			<activity android:name=".presentation.HelpActivity"
				android:label="@string/title_help"
				android:theme="@style/Theme.Main">
			</activity>
									
			<provider android:name=".services.FreeRDPSuggestionProvider"
				android:authorities="com.freerdp.afreerdp.services.freerdpsuggestionprovider"
				>
			</provider>
	
			<receiver android:name=".application.NetworkStateReceiver" android:enabled="true">
			   <intent-filter>
			      <action android:name="android.net.conn.CONNECTIVITY_CHANGE" />
			   </intent-filter>
			</receiver>

								
	</application>
	
</manifest>
