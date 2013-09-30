/*
   Defines base attributes of a bookmark object

   Copyright 2013 Thinstuff Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

import com.freerdp.freerdpcore.application.GlobalApp;

import android.content.SharedPreferences;
import android.os.Parcel;
import android.os.Parcelable;

public class BookmarkBase implements Parcelable, Cloneable
{
	public static final int TYPE_INVALID = -1;
	public static final int TYPE_MANUAL = 1;
	public static final int TYPE_QUICKCONNECT = 2;
	public static final int TYPE_PLACEHOLDER = 3;
	public static final int TYPE_CUSTOM_BASE = 1000;
	
	// performance flags
	public static class PerformanceFlags implements Parcelable
	{
		private boolean remotefx;
		private boolean wallpaper;
		private boolean theming;
		private boolean fullWindowDrag;
		private boolean menuAnimations;
		private boolean fontSmoothing;
		private boolean desktopComposition;
	
		public PerformanceFlags() {
			remotefx = false;
			wallpaper = false;
			theming = false;
			fullWindowDrag = false;
			menuAnimations = false;
			fontSmoothing = false;
			desktopComposition = false;
		}
		
		public PerformanceFlags(Parcel parcel) {
			remotefx = (parcel.readInt() == 1) ? true : false;
			wallpaper = (parcel.readInt() == 1) ? true : false;			
			theming = (parcel.readInt() == 1) ? true : false;			
			fullWindowDrag = (parcel.readInt() == 1) ? true : false;			
			menuAnimations = (parcel.readInt() == 1) ? true : false;			
			fontSmoothing = (parcel.readInt() == 1) ? true : false;
			desktopComposition = (parcel.readInt() == 1) ? true : false;
		}

		public boolean getRemoteFX() {
			return remotefx;
		}
		
		public void setRemoteFX(boolean remotefx) {
			this.remotefx = remotefx;
		}

		public boolean getWallpaper() {
			return wallpaper;
		}
		
		public void setWallpaper(boolean wallpaper) {
			this.wallpaper = wallpaper;
		}
	
		public boolean getTheming() {
			return theming;
		}
		
		public void setTheming(boolean theming) {
			this.theming = theming;
		}

		public boolean getFullWindowDrag() {
			return fullWindowDrag;
		}
		
		public void setFullWindowDrag(boolean fullWindowDrag) {
			this.fullWindowDrag = fullWindowDrag;
		}

		public boolean getMenuAnimations() {
			return menuAnimations;
		}
		
		public void setMenuAnimations(boolean menuAnimations) {
			this.menuAnimations = menuAnimations;
		}

		public boolean getFontSmoothing() {
			return fontSmoothing;
		}
		
		public void setFontSmoothing(boolean fontSmoothing) {
			this.fontSmoothing = fontSmoothing;
		}
		
		public boolean getDesktopComposition() {
			return desktopComposition;
		}
		
		public void setDesktopComposition(boolean desktopComposition) {
			this.desktopComposition = desktopComposition;
		}

		public static final Parcelable.Creator<PerformanceFlags> CREATOR = new Parcelable.Creator<PerformanceFlags>()
		{
			public PerformanceFlags createFromParcel(Parcel in) {
				return new PerformanceFlags(in);
			}

			@Override
			public PerformanceFlags[] newArray(int size) {
				return new PerformanceFlags[size];
			}
		};

		@Override
		public int describeContents() {
			return 0;
		}

		@Override
		public void writeToParcel(Parcel out, int flags)
		{
			out.writeInt(remotefx ? 1 : 0);
			out.writeInt(wallpaper ? 1 : 0);		
			out.writeInt(theming ? 1 : 0);		
			out.writeInt(fullWindowDrag ? 1 : 0);		
			out.writeInt(menuAnimations ? 1 : 0);		
			out.writeInt(fontSmoothing ? 1 : 0);
			out.writeInt(desktopComposition ? 1 : 0);
		}		
	}

	// Screen Settings class
	public static class ScreenSettings implements Parcelable
	{
		public static final int FITSCREEN = -2;
		public static final int AUTOMATIC = -1;
		public static final int CUSTOM = 0;
		public static final int PREDEFINED = 1;
		
		private int resolution;
		private int colors;
		private int width;
		private int height;

		public ScreenSettings() {
			init();
		}

		public ScreenSettings(Parcel parcel) {
			resolution = parcel.readInt();
			colors = parcel.readInt();
			width = parcel.readInt();
			height = parcel.readInt();
		}
				
		private void init() {
			resolution = AUTOMATIC;
			colors = 16;
			width = 0;
			height = 0;
		}
		
		public void setResolution(int resolution)
		{
			this.resolution = resolution;
			
			if (resolution == AUTOMATIC || resolution == FITSCREEN) {
				width = 0;
				height = 0;
			}
		}
		
		public void setResolution(String resolution, int width, int height)
		{
			if (resolution.contains("x")) {
				String[] dimensions = resolution.split("x");
				this.width = Integer.valueOf(dimensions[0]);
				this.height = Integer.valueOf(dimensions[1]);
				this.resolution = PREDEFINED;
			}
			else if (resolution.equalsIgnoreCase("custom"))
			{
				this.width = width;				
				this.height = height;				
				this.resolution = CUSTOM;
			}
			else if (resolution.equalsIgnoreCase("fitscreen"))
			{
				this.width = this.height = 0;
				this.resolution = FITSCREEN;
			}
			else
			{
				this.width = this.height = 0;
				this.resolution = AUTOMATIC;
			}
		}

		public int getResolution()
		{	
			return resolution;
		}
		
		public String getResolutionString()
		{	
			if (isPredefined())
				return (width + "x" + height);

			return (isFitScreen() ? "fitscreen" : isAutomatic() ? "automatic" : "custom");
		}

		public boolean isPredefined() {
			return (resolution == PREDEFINED);			
		}
		
		public boolean isAutomatic() {
			return (resolution == AUTOMATIC);
		}

		public boolean isFitScreen() {
			return (resolution == FITSCREEN);
		}

		public boolean isCustom() {
			return (resolution == CUSTOM);
		}

		public void setWidth(int width) {
			this.width = width;
		}
		
		public int getWidth() {
			return width;
		}
		
		public int getHeight() {
			return height;
		}
		
		public void setHeight(int height) {
			this.height = height;
		}
		
		public void setColors(int colors) {
			this.colors = colors;
		}
		
		public int getColors() {
			return colors;
		}
	
		public static final Parcelable.Creator<ScreenSettings> CREATOR = new Parcelable.Creator<ScreenSettings>()
		{
			public ScreenSettings createFromParcel(Parcel in) {
				return new ScreenSettings(in);
			}

			@Override
			public ScreenSettings[] newArray(int size) {
				return new ScreenSettings[size];
			}
		};

		@Override
		public int describeContents() {
			return 0;
		}

		@Override
		public void writeToParcel(Parcel out, int flags)
		{
			out.writeInt(resolution);
			out.writeInt(colors);
			out.writeInt(width);
			out.writeInt(height);
		}		
	}
	
	
	// Session Settings
	public static class AdvancedSettings implements Parcelable
	{
		private boolean enable3GSettings;
		private ScreenSettings screen3G;
		private PerformanceFlags performance3G;
		private boolean redirectSDCard;
		private int security;
		private boolean consoleMode;
		private String remoteProgram;
		private String workDir;

		public AdvancedSettings() {
			init();
		}

		public AdvancedSettings(Parcel parcel) {
			enable3GSettings = (parcel.readInt() == 1) ? true : false;
			screen3G = parcel.readParcelable(ScreenSettings.class.getClassLoader());
			performance3G = parcel.readParcelable(PerformanceFlags.class.getClassLoader());
			redirectSDCard = (parcel.readInt() == 1) ? true : false;
			security = parcel.readInt();
			consoleMode = (parcel.readInt() == 1) ? true : false;
			remoteProgram = parcel.readString();
			workDir = parcel.readString();			
		}
				
		private void init() {
			enable3GSettings = false;
			screen3G = new ScreenSettings();
			performance3G = new PerformanceFlags();
			redirectSDCard = false;
			security = 0;
			consoleMode = false;
			remoteProgram = "";
			workDir = "";
		}
		
		public void setEnable3GSettings(boolean enable3GSettings) {
			this.enable3GSettings = enable3GSettings;
		}
		
		public boolean getEnable3GSettings() {
			return enable3GSettings;
		}

		public ScreenSettings getScreen3G() {
			return screen3G;
		}
		
		public void setScreen3G(ScreenSettings screen3G) {
			this.screen3G = screen3G; 
		}

		public PerformanceFlags getPerformance3G() {
			return performance3G;
		}
		
		public void setPerformance3G(PerformanceFlags performance3G) {
			this.performance3G = performance3G; 
		}

		public void setRedirectSDCard(boolean redirectSDCard) {
			this.redirectSDCard = redirectSDCard;
		}
		
		public boolean getRedirectSDCard() {
			return redirectSDCard;
		}
		
		public void setSecurity(int security) {
			this.security = security;
		}
		
		public int getSecurity() {
			return security;
		}
		
		public void setConsoleMode(boolean consoleMode) {
			this.consoleMode = consoleMode;
		}
		
		public boolean getConsoleMode() {
			return consoleMode;
		}
		
		public void setRemoteProgram(String remoteProgram)
		{
			this.remoteProgram = remoteProgram;
		}
		
		public String getRemoteProgram()
		{	
			return remoteProgram;
		}
		
		public void setWorkDir(String workDir)
		{
			this.workDir = workDir;
		}
		
		public String getWorkDir()
		{	
			return workDir;
		}		
		
		public static final Parcelable.Creator<AdvancedSettings> CREATOR = new Parcelable.Creator<AdvancedSettings>()
		{
			public AdvancedSettings createFromParcel(Parcel in) {
				return new AdvancedSettings(in);
			}

			@Override
			public AdvancedSettings[] newArray(int size) {
				return new AdvancedSettings[size];
			}
		};

		@Override
		public int describeContents() {
			return 0;
		}

		@Override
		public void writeToParcel(Parcel out, int flags)
		{
			out.writeInt(enable3GSettings ? 1 : 0);
			out.writeParcelable(screen3G, flags);
			out.writeParcelable(performance3G, flags);
			out.writeInt(redirectSDCard ? 1 : 0);
			out.writeInt(security);		
			out.writeInt(consoleMode ? 1 : 0);		
			out.writeString(remoteProgram);
			out.writeString(workDir);
		}		
	}
	
	protected int  type;
	private long   id;
	private String label;
	private String username;
	private String password;
	private String domain;

	private ScreenSettings screenSettings;
	private PerformanceFlags performanceFlags;
	private AdvancedSettings advancedSettings;
	
	private void init()
	{
		type = TYPE_INVALID;
		id = -1;
		label = "";
		username = "";
		password = "";
		domain  = "";
		
		screenSettings = new ScreenSettings();
		performanceFlags = new PerformanceFlags();
		advancedSettings = new AdvancedSettings();
	}
	
	public BookmarkBase(Parcel parcel)
	{
		type = parcel.readInt();
		id = parcel.readLong();
		label = parcel.readString();
		username = parcel.readString();
		password = parcel.readString();
		domain = parcel.readString();

		screenSettings = parcel.readParcelable(ScreenSettings.class.getClassLoader());
		performanceFlags = parcel.readParcelable(PerformanceFlags.class.getClassLoader());
		advancedSettings = parcel.readParcelable(AdvancedSettings.class.getClassLoader());
	}
		
	public BookmarkBase() {
		init();
	}

	@SuppressWarnings("unchecked")
	public <T extends BookmarkBase> T get() { return (T)this; }

	public int getType() {
		return type;
	}
	
	public void setId(long id) {
		this.id = id;
	}
	
	public long getId() {
		return id;
	}
	
	public void setLabel(String label) {
		this.label = label;
	}
	
	public String getLabel() {
		return label;
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
	
	public void setScreenSettings(ScreenSettings screenSettings) {
		this.screenSettings = screenSettings;
	}

	public ScreenSettings getScreenSettings() {
		return screenSettings;
	}

	public void setPerformanceFlags(PerformanceFlags performanceFlags) {
		this.performanceFlags = performanceFlags;
	}
	
	public PerformanceFlags getPerformanceFlags() {
		return performanceFlags;
	}
	
	public void setAdvancedSettings(AdvancedSettings advancedSettings) {
		this.advancedSettings = advancedSettings;
	}

	public AdvancedSettings getAdvancedSettings() {
		return advancedSettings;
	}
	
	public ScreenSettings getActiveScreenSettings()
	{
		return (GlobalApp.ConnectedTo3G && advancedSettings.getEnable3GSettings()) ? advancedSettings.getScreen3G() : screenSettings; 
	}
	
	public PerformanceFlags getActivePerformanceFlags()
	{
		return (GlobalApp.ConnectedTo3G && advancedSettings.getEnable3GSettings()) ? advancedSettings.getPerformance3G() : performanceFlags; 
	}

	public static final Parcelable.Creator<BookmarkBase> CREATOR = new Parcelable.Creator<BookmarkBase>()
	{
		public BookmarkBase createFromParcel(Parcel in) {
			return new BookmarkBase(in);
		}

		@Override
		public BookmarkBase[] newArray(int size) {
			return new BookmarkBase[size];
		}
	};
	
	@Override
	public int describeContents() {
		return 0;
	}

	@Override
	public void writeToParcel(Parcel out, int flags)
	{
		out.writeInt(type);
		out.writeLong(id);
		out.writeString(label);
		out.writeString(username);
		out.writeString(password);
		out.writeString(domain);

		out.writeParcelable(screenSettings, flags);
		out.writeParcelable(performanceFlags, flags);
		out.writeParcelable(advancedSettings, flags);
	}

	// write to shared preferences
	public void writeToSharedPreferences(SharedPreferences sharedPrefs) 
	{
		SharedPreferences.Editor editor = sharedPrefs.edit();
		editor.clear();
		editor.putString("bookmark.label", label);
		editor.putString("bookmark.username", username);
		editor.putString("bookmark.password", password);
		editor.putString("bookmark.domain", domain);

		editor.putInt("bookmark.colors", screenSettings.getColors());
		editor.putString("bookmark.resolution", screenSettings.getResolutionString().toLowerCase());
		editor.putInt("bookmark.width", screenSettings.getWidth());
		editor.putInt("bookmark.height", screenSettings.getHeight());

		editor.putBoolean("bookmark.perf_remotefx", performanceFlags.getRemoteFX());
		editor.putBoolean("bookmark.perf_wallpaper", performanceFlags.getWallpaper());
		editor.putBoolean("bookmark.perf_font_smoothing", performanceFlags.getFontSmoothing());
		editor.putBoolean("bookmark.perf_desktop_composition", performanceFlags.getDesktopComposition());
		editor.putBoolean("bookmark.perf_window_dragging", performanceFlags.getFullWindowDrag());
		editor.putBoolean("bookmark.perf_menu_animation", performanceFlags.getMenuAnimations());
		editor.putBoolean("bookmark.perf_themes", performanceFlags.getTheming());
		
		editor.putBoolean("bookmark.enable_3g_settings", advancedSettings.getEnable3GSettings());

		editor.putInt("bookmark.colors_3g", advancedSettings.getScreen3G().getColors());
		editor.putString("bookmark.resolution_3g", advancedSettings.getScreen3G().getResolutionString().toLowerCase());
		editor.putInt("bookmark.width_3g", advancedSettings.getScreen3G().getWidth());
		editor.putInt("bookmark.height_3g", advancedSettings.getScreen3G().getHeight());

		editor.putBoolean("bookmark.perf_remotefx_3g", advancedSettings.getPerformance3G().getRemoteFX());
		editor.putBoolean("bookmark.perf_wallpaper_3g", advancedSettings.getPerformance3G().getWallpaper());
		editor.putBoolean("bookmark.perf_font_smoothing_3g", advancedSettings.getPerformance3G().getFontSmoothing());
		editor.putBoolean("bookmark.perf_desktop_composition_3g", advancedSettings.getPerformance3G().getDesktopComposition());
		editor.putBoolean("bookmark.perf_window_dragging_3g", advancedSettings.getPerformance3G().getFullWindowDrag());
		editor.putBoolean("bookmark.perf_menu_animation_3g", advancedSettings.getPerformance3G().getMenuAnimations());
		editor.putBoolean("bookmark.perf_themes_3g", advancedSettings.getPerformance3G().getTheming());

		editor.putBoolean("bookmark.redirect_sdcard", advancedSettings.getRedirectSDCard());
		editor.putInt("bookmark.security", advancedSettings.getSecurity());
		editor.putString("bookmark.remote_program", advancedSettings.getRemoteProgram());
		editor.putString("bookmark.work_dir", advancedSettings.getWorkDir());
		editor.putBoolean("bookmark.console_mode", advancedSettings.getConsoleMode());
		
		editor.commit();
	}

	// read from shared preferences
	public void readFromSharedPreferences(SharedPreferences sharedPrefs) 
	{
		label = sharedPrefs.getString("bookmark.label", "");
		username = sharedPrefs.getString("bookmark.username", "");
		password = sharedPrefs.getString("bookmark.password", "");
		domain = sharedPrefs.getString("bookmark.domain", "");

		screenSettings.setColors(sharedPrefs.getInt("bookmark.colors", 16));
		screenSettings.setResolution(sharedPrefs.getString("bookmark.resolution", "automatic"), sharedPrefs.getInt("bookmark.width", 800), sharedPrefs.getInt("bookmark.height", 600));

		performanceFlags.setRemoteFX(sharedPrefs.getBoolean("bookmark.perf_remotefx", false));
		performanceFlags.setWallpaper(sharedPrefs.getBoolean("bookmark.perf_wallpaper", false));
		performanceFlags.setFontSmoothing(sharedPrefs.getBoolean("bookmark.perf_font_smoothing", false));
		performanceFlags.setDesktopComposition(sharedPrefs.getBoolean("bookmark.perf_desktop_composition", false));
		performanceFlags.setFullWindowDrag(sharedPrefs.getBoolean("bookmark.perf_window_dragging", false));
		performanceFlags.setMenuAnimations(sharedPrefs.getBoolean("bookmark.perf_menu_animation", false));
		performanceFlags.setTheming(sharedPrefs.getBoolean("bookmark.perf_themes", false));
		
		advancedSettings.setEnable3GSettings(sharedPrefs.getBoolean("bookmark.enable_3g_settings", false));

		advancedSettings.getScreen3G().setColors(sharedPrefs.getInt("bookmark.colors_3g", 16));
		advancedSettings.getScreen3G().setResolution(sharedPrefs.getString("bookmark.resolution_3g", "automatic"), 
													 sharedPrefs.getInt("bookmark.width_3g", 800), sharedPrefs.getInt("bookmark.height_3g", 600));

		advancedSettings.getPerformance3G().setRemoteFX(sharedPrefs.getBoolean("bookmark.perf_remotefx_3g", false));
		advancedSettings.getPerformance3G().setWallpaper(sharedPrefs.getBoolean("bookmark.perf_wallpaper_3g", false));
		advancedSettings.getPerformance3G().setFontSmoothing(sharedPrefs.getBoolean("bookmark.perf_font_smoothing_3g", false));
		advancedSettings.getPerformance3G().setDesktopComposition(sharedPrefs.getBoolean("bookmark.perf_desktop_composition_3g", false));
		advancedSettings.getPerformance3G().setFullWindowDrag(sharedPrefs.getBoolean("bookmark.perf_window_dragging_3g", false));
		advancedSettings.getPerformance3G().setMenuAnimations(sharedPrefs.getBoolean("bookmark.perf_menu_animation_3g", false));
		advancedSettings.getPerformance3G().setTheming(sharedPrefs.getBoolean("bookmark.perf_themes_3g", false));

		advancedSettings.setRedirectSDCard(sharedPrefs.getBoolean("bookmark.redirect_sdcard", false));
		advancedSettings.setSecurity(sharedPrefs.getInt("bookmark.security", 0));
		advancedSettings.setRemoteProgram(sharedPrefs.getString("bookmark.remote_program", ""));
		advancedSettings.setWorkDir(sharedPrefs.getString("bookmark.work_dir", ""));
		advancedSettings.setConsoleMode(sharedPrefs.getBoolean("bookmark.console_mode", false));
	}

	// Cloneable
	public Object clone()
	{
		try
		{
			return super.clone();					
		}
		catch(CloneNotSupportedException e)
		{
			return null;
		}
	}
}
