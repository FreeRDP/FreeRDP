/*
   Manual Bookmark implementation

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

import android.content.SharedPreferences;
import android.os.Parcel;
import android.os.Parcelable;

public class ManualBookmark extends BookmarkBase
{
    // Gateway Settings class
    public static class GatewaySettings implements Parcelable
    {
		private String hostname;
		private int    port;
		private String username;
		private String password;
		private String domain;
		
		public GatewaySettings() {
	        hostname = "";
			port = 443;
			username = "";
			password = "";
			domain = "";
		}
		
		public GatewaySettings(Parcel parcel) {
	        hostname = parcel.readString();
			port = parcel.readInt();
			username = parcel.readString();
			password = parcel.readString();
			domain = parcel.readString();
		}
		
		public void setHostname(String hostname) {
			this.hostname = hostname;
		}
		
		public String getHostname() {
			return hostname;
		}
		
		public void setPort(int port) {
			this.port = port;
		}
		
		public int getPort() {
			return port;
		}
		
		public void setUsername(String username) {
	        this.username = username;
		}
		
		public String getUsername() {
	        return username;
		}
		
		public void setPassword(String password) {
			this.password = password;
		}
		
		public String getPassword() {
			return password;
		}
		
		public void setDomain(String domain) {
			this.domain = domain;
		}
		
		public String getDomain() {
			return domain;
		}
		
		@Override
		public int describeContents() {
			return 0;
		}
		
		@Override
		public void writeToParcel(Parcel out, int flags)
		{
			out.writeString(hostname);
			out.writeInt(port);
			out.writeString(username);
			out.writeString(password);
			out.writeString(domain);
		}
		
		public static final Parcelable.Creator<GatewaySettings> CREATOR = new Parcelable.Creator<GatewaySettings>()
		{
			public GatewaySettings createFromParcel(Parcel in) {
			        return new GatewaySettings(in);
			}
			
			@Override
			public GatewaySettings[] newArray(int size) {
			        return new GatewaySettings[size];
			}
		};      
    }
	
	private String hostname;
	private int port;
	private boolean enableGatewaySettings;
	private GatewaySettings gatewaySettings;

	private void init()
	{
		type = TYPE_MANUAL;
		hostname = "";
		port = 3389;
		enableGatewaySettings = false;
		gatewaySettings = new GatewaySettings();
	}
	
	public ManualBookmark(Parcel parcel)
	{
		super(parcel);
		type = TYPE_MANUAL;
		hostname = parcel.readString();
		port = parcel.readInt();		
		
		enableGatewaySettings = (parcel.readInt() == 1 ? true : false);		
		gatewaySettings = parcel.readParcelable(GatewaySettings.class.getClassLoader());
	}

	public ManualBookmark() {
		super();
		init();
	}

	public void setHostname(String hostname) {
		this.hostname = hostname;
	}
	
	public String getHostname() {
		return hostname;
	}
	
	public void setPort(int port) {
		this.port = port;
	}
	
	public int getPort() {
		return port;
	}
	
	public boolean getEnableGatewaySettings()
	{
		return enableGatewaySettings;
	}
	
	public void setEnableGatewaySettings(boolean enableGatewaySettings)
	{
		this.enableGatewaySettings = enableGatewaySettings;
	}
	
	public GatewaySettings getGatewaySettings()
	{
		return gatewaySettings;
	}

	public void setGatewaySettings(GatewaySettings gatewaySettings)
	{
		this.gatewaySettings = gatewaySettings;
	}
	
	public static final Parcelable.Creator<ManualBookmark> CREATOR = new Parcelable.Creator<ManualBookmark>()
	{
		public ManualBookmark createFromParcel(Parcel in) {
			return new ManualBookmark(in);
		}

		@Override
		public ManualBookmark[] newArray(int size) {
			return new ManualBookmark[size];
		}
	};
	
	@Override
	public int describeContents() {
		return 0;
	}

	@Override
	public void writeToParcel(Parcel out, int flags)
	{
		super.writeToParcel(out, flags);
		out.writeString(hostname);
		out.writeInt(port);
		out.writeInt(enableGatewaySettings ? 1 : 0);
		out.writeParcelable(gatewaySettings, flags);
	}

	@Override
	public void writeToSharedPreferences(SharedPreferences sharedPrefs)
	{
		super.writeToSharedPreferences(sharedPrefs);
		
		SharedPreferences.Editor editor = sharedPrefs.edit();
		editor.putString("bookmark.hostname", hostname);
		editor.putInt("bookmark.port", port);
		editor.putBoolean("bookmark.enable_gateway_settings", enableGatewaySettings);
		editor.putString("bookmark.gateway_hostname", gatewaySettings.getHostname());		
		editor.putInt("bookmark.gateway_port", gatewaySettings.getPort());		
		editor.putString("bookmark.gateway_username", gatewaySettings.getUsername());		
		editor.putString("bookmark.gateway_password", gatewaySettings.getPassword());		
		editor.putString("bookmark.gateway_domain", gatewaySettings.getDomain());		
		editor.commit();		
	}

	@Override
	public void readFromSharedPreferences(SharedPreferences sharedPrefs)
	{
		super.readFromSharedPreferences(sharedPrefs);
		
		hostname = sharedPrefs.getString("bookmark.hostname", "");
		port = sharedPrefs.getInt("bookmark.port", 3389);
		enableGatewaySettings = sharedPrefs.getBoolean("bookmark.enable_gateway_settings", false);
		gatewaySettings.setHostname(sharedPrefs.getString("bookmark.gateway_hostname", ""));
		gatewaySettings.setPort(sharedPrefs.getInt("bookmark.gateway_port", 443));
		gatewaySettings.setUsername(sharedPrefs.getString("bookmark.gateway_username", ""));
		gatewaySettings.setPassword(sharedPrefs.getString("bookmark.gateway_password", ""));
		gatewaySettings.setDomain(sharedPrefs.getString("bookmark.gateway_domain", ""));
	}
	
	// Cloneable
	public Object clone()
	{
		return super.clone();					
	}
}
