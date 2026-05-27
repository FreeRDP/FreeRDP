/*
   Defines base attributes of a bookmark object

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.domain;

import android.content.SharedPreferences;
import android.os.Parcel;
import android.os.Parcelable;

import androidx.annotation.NonNull;

import java.util.Locale;
import java.util.Objects;

public class BookmarkBase implements Parcelable, Cloneable
{
	private static final String keyLabel = "bookmark.label";
	private static final String keyUsername = "bookmark.username";
	private static final String keyPassword = "bookmark.password";
	private static final String keyDomain = "bookmark.domain";

	private static final String keyColors = "bookmark.colors";
	private static final String keyResolution = "bookmark.resolution";
	private static final String keyWidth = "bookmark.width";
	private static final String keyScaleMode = "bookmark.scale_mode";
	private static final String keyScaleDesktop = "bookmark.scale_desktop";
	private static final String keyScaleDevice = "bookmark.scale_device";
	private static final String keyHeight = "bookmark.height";

	private static final String keyRFX = "bookmark.perf_remotefx";
	private static final String keyGFX = "bookmark.perf_gfx";
	private static final String keyH264 = "bookmark.perf_gfx_h264";
	private static final String keyFlagWallpaper = "bookmark.perf_wallpaper";
	private static final String keyFlagFonts = "bookmark.perf_font_smoothing";
	private static final String keyFlagComposition = "bookmark.perf_desktop_composition";
	private static final String keyFlagWindowDrag = "bookmark.perf_window_dragging";
	private static final String keyFlagMenuAnim = "bookmark.perf_menu_animation";
	private static final String keyFlagTheming = "bookmark.perf_themes";

	private static final String keyTlsSecLevel = "bookmark.tlsSecLevel";
	private static final String keyTlsMinLevel = "bookmark.tlsMinLevel";
	private static final String keyLoadBalanceInfo = "bookmark.loadBalanceInfo";
	private static final String keyRedirectSDCard = "bookmark.redirect_sdcard";
	private static final String keySound = "bookmark.redirect_sound";
	private static final String keyMicrophone = "bookmark.redirect_microphone";
	private static final String keyPrinter = "bookmark.redirect_printer";
	private static final String keySecurity = "bookmark.security";
	private static final String keyRemoteApp = "bookmark.remote_program";
	private static final String keyWorkDir = "bookmark.work_dir";
	private static final String keyConsoleMode = "bookmark.console_mode";
	private static final String keyVmConnectMode = "bookmark.vmconnect_mode";
	private static final String keyVmConnectGuid = "bookmark.vmconnect_guid";

	private static final String keyAsyncChannel = "bookmark.async_channel";
	private static final String keyAsyncUpdate = "bookmark.async_update";
	private static final String keyDebugLevel = "bookmark.debug_level";

	private static final String keyHostname = "bookmark.hostname";
	private static final String keyPort = "bookmark.port";
	private static final String keyGatewayEnabled = "bookmark.enable_gateway_settings";
	private static final String keyGatewayHostname = "bookmark.gateway_hostname";
	private static final String keyGatewayPort = "bookmark.gateway_port";
	private static final String keyGatewyUser = "bookmark.gateway_username";
	private static final String keyGatewayPassword = "bookmark.gateway_password";
	private static final String keyGatewayDomain = "bookmark.gateway_domain";

	public static final int TYPE_INVALID = -1;
	public static final int TYPE_MANUAL = 1;
	public static final int TYPE_QUICKCONNECT = 2;
	public static final int TYPE_CUSTOM_BASE = 1000;
	public static final Parcelable.Creator<BookmarkBase> CREATOR =
	    new Parcelable.Creator<BookmarkBase>() {
		    public BookmarkBase createFromParcel(Parcel in)
		    {
			    return new BookmarkBase(in);
		    }

		    @Override public BookmarkBase[] newArray(int size)
		    {
			    return new BookmarkBase[size];
		    }
	    };
	protected int type = TYPE_MANUAL;
	private long id = -1;

	@NonNull private String label = "";

	@NonNull private String username = "";

	@NonNull private String password = "";

	@NonNull private String domain = "";

	@NonNull private ScreenSettings screenSettings = new ScreenSettings();

	@NonNull private PerformanceFlags performanceFlags = new PerformanceFlags();

	@NonNull private AdvancedSettings advancedSettings = new AdvancedSettings();

	@NonNull private DebugSettings debugSettings = new DebugSettings();

	@NonNull private String hostname = "";
	private int port = 3389;
	private boolean enableGatewaySettings = false;

	@NonNull private GatewaySettings gatewaySettings = new GatewaySettings();
	private boolean directConnect = false;

	public BookmarkBase(Parcel parcel)
	{
		type = parcel.readInt();
		id = parcel.readLong();
		label = Objects.requireNonNull(parcel.readString());
		username = Objects.requireNonNull(parcel.readString());
		password = Objects.requireNonNull(parcel.readString());
		domain = Objects.requireNonNull(parcel.readString());

		screenSettings =
		    Objects.requireNonNull(parcel.readParcelable(ScreenSettings.class.getClassLoader()));
		performanceFlags =
		    Objects.requireNonNull(parcel.readParcelable(PerformanceFlags.class.getClassLoader()));
		advancedSettings =
		    Objects.requireNonNull(parcel.readParcelable(AdvancedSettings.class.getClassLoader()));
		debugSettings =
		    Objects.requireNonNull(parcel.readParcelable(DebugSettings.class.getClassLoader()));
		hostname = Objects.requireNonNull(parcel.readString());
		port = parcel.readInt();
		enableGatewaySettings = (parcel.readInt() == 1);
		gatewaySettings =
		    Objects.requireNonNull(parcel.readParcelable(GatewaySettings.class.getClassLoader()));
		directConnect = (parcel.readInt() == 1);
	}

	public BookmarkBase()
	{
	}

	public int getType()
	{
		return type;
	}

	public void setType(int type)
	{
		this.type = type;
	}

	public long getId()
	{
		return id;
	}

	public void setId(long id)
	{
		this.id = id;
	}

	@NonNull public String getLabel()
	{
		return label;
	}

	public void setLabel(@NonNull String label)
	{
		this.label = label;
	}

	@NonNull public String getUsername()
	{
		return username;
	}

	public void setUsername(@NonNull String username)
	{
		this.username = username;
	}

	@NonNull public String getPassword()
	{
		return password;
	}

	public void setPassword(@NonNull String password)
	{
		this.password = password;
	}

	@NonNull public String getDomain()
	{
		return domain;
	}

	public void setDomain(@NonNull String domain)
	{
		this.domain = domain;
	}

	@NonNull public ScreenSettings getScreenSettings()
	{
		return screenSettings;
	}

	@NonNull public PerformanceFlags getPerformanceFlags()
	{
		return performanceFlags;
	}

	@NonNull public AdvancedSettings getAdvancedSettings()
	{
		return advancedSettings;
	}

	@NonNull public DebugSettings getDebugSettings()
	{
		return debugSettings;
	}

	@NonNull public String getHostname()
	{
		return hostname;
	}

	public void setHostname(@NonNull String hostname)
	{
		this.hostname = hostname;
	}

	public int getPort()
	{
		return port;
	}

	public void setPort(int port)
	{
		this.port = port;
	}

	public boolean getEnableGatewaySettings()
	{
		return enableGatewaySettings;
	}

	public void setEnableGatewaySettings(boolean enableGatewaySettings)
	{
		this.enableGatewaySettings = enableGatewaySettings;
	}

	@NonNull public GatewaySettings getGatewaySettings()
	{
		return gatewaySettings;
	}

	public boolean isDirectConnect()
	{
		return directConnect;
	}

	public void setDirectConnect(boolean directConnect)
	{
		this.directConnect = directConnect;
	}

	@NonNull public ScreenSettings getActiveScreenSettings()
	{
		return screenSettings;
	}

	@NonNull public PerformanceFlags getActivePerformanceFlags()
	{
		return performanceFlags;
	}

	@Override public int describeContents()
	{
		return 0;
	}

	@Override public void writeToParcel(Parcel out, int flags)
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
		out.writeParcelable(debugSettings, flags);
		out.writeString(hostname);
		out.writeInt(port);
		out.writeBoolean(enableGatewaySettings);
		out.writeParcelable(gatewaySettings, flags);
		out.writeBoolean(directConnect);
	}

	// write to shared preferences
	public void writeToSharedPreferences(@NonNull SharedPreferences sharedPrefs)
	{
		Locale locale = Locale.ENGLISH;

		SharedPreferences.Editor editor = sharedPrefs.edit();
		editor.clear();
		editor.putString(keyLabel, label);
		editor.putString(keyUsername, username);
		editor.putString(keyPassword, password);
		editor.putString(keyDomain, domain);

		editor.putInt(keyColors, screenSettings.getColors());
		editor.putString(keyResolution, screenSettings.getResolutionString().toLowerCase(locale));
		editor.putInt(keyWidth, screenSettings.getWidth());
		editor.putInt(keyHeight, screenSettings.getHeight());
		editor.putString(keyScaleMode, screenSettings.getScaleMode());
		editor.putInt(keyScaleDesktop, screenSettings.getScaleDesktop());
		editor.putInt(keyScaleDevice, screenSettings.getScaleDevice());

		editor.putBoolean(keyRFX, performanceFlags.getRemoteFX());
		editor.putBoolean(keyGFX, performanceFlags.getGfx());
		editor.putBoolean(keyH264, performanceFlags.getH264());
		editor.putBoolean(keyFlagWallpaper, performanceFlags.getWallpaper());
		editor.putBoolean(keyFlagFonts, performanceFlags.getFontSmoothing());
		editor.putBoolean(keyFlagComposition, performanceFlags.getDesktopComposition());
		editor.putBoolean(keyFlagWindowDrag, performanceFlags.getFullWindowDrag());
		editor.putBoolean(keyFlagMenuAnim, performanceFlags.getMenuAnimations());
		editor.putBoolean(keyFlagTheming, performanceFlags.getTheming());

		editor.putInt(keyTlsSecLevel, advancedSettings.tlsSecLevel);
		editor.putInt(keyTlsMinLevel, advancedSettings.tlsMinLevel);

		editor.putString(keyLoadBalanceInfo, advancedSettings.getLoadBalanceInfo());
		editor.putBoolean(keyRedirectSDCard, advancedSettings.getRedirectSDCard());
		editor.putInt(keySound, advancedSettings.getRedirectSound());
		editor.putBoolean(keyMicrophone, advancedSettings.getRedirectMicrophone());
		editor.putBoolean(keyPrinter, advancedSettings.getRedirectPrinter());
		editor.putInt(keySecurity, advancedSettings.getSecurity());
		editor.putString(keyRemoteApp, advancedSettings.getRemoteProgram());
		editor.putString(keyWorkDir, advancedSettings.getWorkDir());
		editor.putBoolean(keyConsoleMode, advancedSettings.getConsoleMode());
		editor.putBoolean(keyVmConnectMode, advancedSettings.getVmConnectMode());
		editor.putString(keyVmConnectGuid, advancedSettings.getVmConnectGuid());

		editor.putBoolean(keyAsyncChannel, debugSettings.getAsyncChannel());
		editor.putBoolean(keyAsyncUpdate, debugSettings.getAsyncUpdate());
		editor.putString(keyDebugLevel, debugSettings.getDebugLevel());

		editor.putString(keyHostname, hostname);
		editor.putInt(keyPort, port);
		editor.putBoolean(keyGatewayEnabled, enableGatewaySettings);
		editor.putString(keyGatewayHostname, gatewaySettings.getHostname());
		editor.putInt(keyGatewayPort, gatewaySettings.getPort());
		editor.putString(keyGatewyUser, gatewaySettings.getUsername());
		editor.putString(keyGatewayPassword, gatewaySettings.getPassword());
		editor.putString(keyGatewayDomain, gatewaySettings.getDomain());

		editor.apply();
	}

	// read from shared preferences
	public void readFromSharedPreferences(@NonNull SharedPreferences sharedPrefs)
	{
		label = sharedPrefs.getString(keyLabel, "");
		username = sharedPrefs.getString(keyUsername, "");
		password = sharedPrefs.getString(keyPassword, "");
		domain = sharedPrefs.getString(keyDomain, "");

		screenSettings.setColors(sharedPrefs.getInt(keyColors, 32));
		screenSettings.setResolution(sharedPrefs.getString(keyResolution, "automatic"),
		                             sharedPrefs.getInt(keyWidth, 800),
		                             sharedPrefs.getInt(keyHeight, 600));
		screenSettings.setScale(sharedPrefs.getString(keyScaleMode, "100"),
		                        sharedPrefs.getInt(keyScaleDesktop, 100),
		                        sharedPrefs.getInt(keyScaleDevice, 100));

		performanceFlags.setRemoteFX(sharedPrefs.getBoolean(keyRFX, false));
		performanceFlags.setGfx(sharedPrefs.getBoolean(keyGFX, true));
		performanceFlags.setH264(sharedPrefs.getBoolean(keyH264, true));
		performanceFlags.setWallpaper(sharedPrefs.getBoolean(keyFlagWallpaper, false));
		performanceFlags.setFontSmoothing(sharedPrefs.getBoolean(keyFlagFonts, false));
		performanceFlags.setDesktopComposition(sharedPrefs.getBoolean(keyFlagComposition, false));
		performanceFlags.setFullWindowDrag(sharedPrefs.getBoolean(keyFlagWindowDrag, false));
		performanceFlags.setMenuAnimations(sharedPrefs.getBoolean(keyFlagMenuAnim, false));
		performanceFlags.setTheming(sharedPrefs.getBoolean(keyFlagTheming, false));

		advancedSettings.setTlsSecLevel(sharedPrefs.getInt(keyTlsSecLevel, -1));
		advancedSettings.setTlsMinLevel(sharedPrefs.getInt(keyTlsMinLevel, -1));

		advancedSettings.setLoadBalanceInfo(sharedPrefs.getString(keyLoadBalanceInfo, ""));
		advancedSettings.setRedirectSDCard(sharedPrefs.getBoolean(keyRedirectSDCard, false));
		advancedSettings.setRedirectSound(sharedPrefs.getInt(keySound, 0));
		advancedSettings.setRedirectMicrophone(sharedPrefs.getBoolean(keyMicrophone, false));
		advancedSettings.setRedirectPrinter(sharedPrefs.getBoolean(keyPrinter, false));
		advancedSettings.setSecurity(sharedPrefs.getInt(keySecurity, 0));
		advancedSettings.setRemoteProgram(sharedPrefs.getString(keyRemoteApp, ""));
		advancedSettings.setWorkDir(sharedPrefs.getString(keyWorkDir, ""));
		advancedSettings.setConsoleMode(sharedPrefs.getBoolean(keyConsoleMode, false));
		advancedSettings.setVmConnectMode(sharedPrefs.getBoolean(keyVmConnectMode, false));
		advancedSettings.setVmConnectGuid(sharedPrefs.getString(keyVmConnectGuid, ""));

		debugSettings.setAsyncChannel(sharedPrefs.getBoolean(keyAsyncChannel, true));
		debugSettings.setAsyncUpdate(sharedPrefs.getBoolean(keyAsyncUpdate, true));
		debugSettings.setDebugLevel(sharedPrefs.getString(keyDebugLevel, "INFO"));

		hostname = sharedPrefs.getString(keyHostname, "");
		port = sharedPrefs.getInt(keyPort, 3389);
		enableGatewaySettings = sharedPrefs.getBoolean(keyGatewayEnabled, false);
		gatewaySettings.setHostname(sharedPrefs.getString(keyGatewayHostname, ""));
		gatewaySettings.setPort(sharedPrefs.getInt(keyGatewayPort, 443));
		gatewaySettings.setUsername(sharedPrefs.getString(keyGatewyUser, ""));
		gatewaySettings.setPassword(sharedPrefs.getString(keyGatewayPassword, ""));
		gatewaySettings.setDomain(sharedPrefs.getString(keyGatewayDomain, ""));
	}

	@Override public Object clone() throws CloneNotSupportedException
	{
		return super.clone();
	}

	// performance flags
	public static class PerformanceFlags implements Parcelable
	{
		public static final Parcelable.Creator<PerformanceFlags> CREATOR =
		    new Parcelable.Creator<PerformanceFlags>() {
			    @NonNull public PerformanceFlags createFromParcel(Parcel in)
			    {
				    return new PerformanceFlags(in);
			    }

			    @NonNull @Override public PerformanceFlags[] newArray(int size)
			    {
				    return new PerformanceFlags[size];
			    }
		    };
		private boolean remotefx = true;
		private boolean gfx = true;
		private boolean h264 = true;
		private boolean wallpaper = true;
		private boolean theming = true;
		private boolean fullWindowDrag = true;
		private boolean menuAnimations = true;
		private boolean fontSmoothing = true;
		private boolean desktopComposition = true;

		public PerformanceFlags()
		{
		}

		public PerformanceFlags(Parcel parcel)
		{
			remotefx = parcel.readBoolean();
			gfx = parcel.readBoolean();
			h264 = parcel.readBoolean();
			wallpaper = parcel.readBoolean();
			theming = parcel.readBoolean();
			fullWindowDrag = parcel.readBoolean();
			menuAnimations = parcel.readBoolean();
			fontSmoothing = parcel.readBoolean();
			desktopComposition = parcel.readBoolean();
		}

		public boolean getRemoteFX()
		{
			return remotefx;
		}

		public void setRemoteFX(boolean remotefx)
		{
			this.remotefx = remotefx;
		}

		public boolean getGfx()
		{
			return gfx;
		}

		public void setGfx(boolean gfx)
		{
			this.gfx = gfx;
		}

		public boolean getH264()
		{
			return h264;
		}

		public void setH264(boolean h264)
		{
			this.h264 = h264;
		}

		public boolean getWallpaper()
		{
			return wallpaper;
		}

		public void setWallpaper(boolean wallpaper)
		{
			this.wallpaper = wallpaper;
		}

		public boolean getTheming()
		{
			return theming;
		}

		public void setTheming(boolean theming)
		{
			this.theming = theming;
		}

		public boolean getFullWindowDrag()
		{
			return fullWindowDrag;
		}

		public void setFullWindowDrag(boolean fullWindowDrag)
		{
			this.fullWindowDrag = fullWindowDrag;
		}

		public boolean getMenuAnimations()
		{
			return menuAnimations;
		}

		public void setMenuAnimations(boolean menuAnimations)
		{
			this.menuAnimations = menuAnimations;
		}

		public boolean getFontSmoothing()
		{
			return fontSmoothing;
		}

		public void setFontSmoothing(boolean fontSmoothing)
		{
			this.fontSmoothing = fontSmoothing;
		}

		public boolean getDesktopComposition()
		{
			return desktopComposition;
		}

		public void setDesktopComposition(boolean desktopComposition)
		{
			this.desktopComposition = desktopComposition;
		}

		@Override public int describeContents()
		{
			return 0;
		}

		@Override public void writeToParcel(@NonNull Parcel out, int flags)
		{
			out.writeBoolean(remotefx);
			out.writeBoolean(gfx);
			out.writeBoolean(h264);
			out.writeBoolean(wallpaper);
			out.writeBoolean(theming);
			out.writeBoolean(fullWindowDrag);
			out.writeBoolean(menuAnimations);
			out.writeBoolean(fontSmoothing);
			out.writeBoolean(desktopComposition);
		}
	}

	// Screen Settings class
	public static class ScreenSettings implements Parcelable
	{
		public static final int FITSCREEN = -2;
		public static final int AUTOMATIC = -1;
		public static final int CUSTOM = 0;
		public static final int PREDEFINED = 1;
		public static final Parcelable.Creator<ScreenSettings> CREATOR =
		    new Parcelable.Creator<ScreenSettings>() {
			    @NonNull public ScreenSettings createFromParcel(Parcel in)
			    {
				    return new ScreenSettings(in);
			    }

			    @NonNull @Override public ScreenSettings[] newArray(int size)
			    {
				    return new ScreenSettings[size];
			    }
		    };
		private int resolution = AUTOMATIC;
		private int colors = 32;
		private int width = 0;
		private int height = 0;
		private String scaleMode = "100";
		private int scaleDesktop = 100;
		private int scaleDevice = 100;

		public ScreenSettings()
		{
		}

		public ScreenSettings(Parcel parcel)
		{
			resolution = parcel.readInt();
			colors = parcel.readInt();
			width = parcel.readInt();
			height = parcel.readInt();
			scaleMode = parcel.readString();
			scaleDesktop = parcel.readInt();
			scaleDevice = parcel.readInt();
		}

		private void validate()
		{
			switch (colors)
			{
				case 32:
				case 24:
				case 16:
				case 15:
				case 8:
					break;
				default:
					colors = 32;
					break;
			}

			if ((width <= 0) || (width > 65536))
			{
				width = 1024;
			}

			if ((height <= 0) || (height > 65536))
			{
				height = 768;
			}

			switch (resolution)
			{
				case FITSCREEN:
				case AUTOMATIC:
				case CUSTOM:
				case PREDEFINED:
					break;
				default:
					resolution = AUTOMATIC;
					break;
			}
		}

		public void setResolution(@NonNull String resolution, int width, int height)
		{
			if (resolution.contains("x"))
			{
				String[] dimensions = resolution.split("x");
				this.width = Integer.parseInt(dimensions[0]);
				this.height = Integer.parseInt(dimensions[1]);
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

		public void setResolution(int resolution)
		{
			this.resolution = resolution;

			if (resolution == AUTOMATIC || resolution == FITSCREEN)
			{
				width = 0;
				height = 0;
			}
		}

		@NonNull public String getResolutionString()
		{
			if (isPredefined())
				return (width + "x" + height);

			return (isFitScreen() ? "fitscreen" : isAutomatic() ? "automatic" : "custom");
		}

		public boolean isPredefined()
		{
			validate();
			return (resolution == PREDEFINED);
		}

		public boolean isAutomatic()
		{
			validate();
			return (resolution == AUTOMATIC);
		}

		public boolean isFitScreen()
		{
			validate();
			return (resolution == FITSCREEN);
		}

		public boolean isCustom()
		{
			validate();
			return (resolution == CUSTOM);
		}

		public int getWidth()
		{
			validate();
			return width;
		}

		public void setWidth(int width)
		{
			this.width = width;
		}

		public int getHeight()
		{
			validate();
			return height;
		}

		public void setHeight(int height)
		{
			this.height = height;
		}

		public int getColors()
		{
			validate();
			return colors;
		}

		public void setColors(int colors)
		{
			this.colors = colors;
		}

		public void setScale(@NonNull String mode, int desktop, int device)
		{
			scaleMode = "custom".equalsIgnoreCase(mode) ? "custom" : mode;
			scaleDesktop = desktop;
			scaleDevice = device;
		}

		public boolean isCustomScale()
		{
			return "custom".equalsIgnoreCase(scaleMode);
		}

		@NonNull public String getScaleMode()
		{
			return scaleMode;
		}

		public int getScalePreset()
		{
			if ("140".equals(scaleMode))
				return 140;
			if ("180".equals(scaleMode))
				return 180;
			return 100;
		}

		public int getScaleDesktop()
		{
			return scaleDesktop;
		}

		public int getScaleDevice()
		{
			return scaleDevice;
		}

		@Override public int describeContents()
		{
			return 0;
		}

		@Override public void writeToParcel(@NonNull Parcel out, int flags)
		{
			out.writeInt(resolution);
			out.writeInt(colors);
			out.writeInt(width);
			out.writeInt(height);
			out.writeString(scaleMode);
			out.writeInt(scaleDesktop);
			out.writeInt(scaleDevice);
		}
	}

	public static class DebugSettings implements Parcelable
	{
		public static final Parcelable.Creator<DebugSettings> CREATOR =
		    new Parcelable.Creator<DebugSettings>() {
			    @NonNull public DebugSettings createFromParcel(Parcel in)
			    {
				    return new DebugSettings(in);
			    }

			    @NonNull @Override public DebugSettings[] newArray(int size)
			    {
				    return new DebugSettings[size];
			    }
		    };

		@NonNull private String debug = "INFO";
		private boolean asyncChannel = true;
		private boolean asyncTransport = false;
		private boolean asyncUpdate = true;

		public DebugSettings()
		{
		}

		// Session Settings
		public DebugSettings(@NonNull Parcel parcel)
		{
			asyncChannel = parcel.readBoolean();
			asyncTransport = parcel.readBoolean();
			asyncUpdate = parcel.readBoolean();
			debug = Objects.requireNonNull(parcel.readString());
		}

		private void validate()
		{
			final String[] levels = { "OFF", "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };

			for (String level : levels)
			{
				if (level.equalsIgnoreCase(this.debug))
				{
					return;
				}
			}

			this.debug = "INFO";
		}

		@NonNull public String getDebugLevel()
		{
			validate();
			return debug;
		}

		public void setDebugLevel(@NonNull String debug)
		{
			this.debug = debug;
		}

		public boolean getAsyncUpdate()
		{
			return asyncUpdate;
		}

		public void setAsyncUpdate(boolean enabled)
		{
			asyncUpdate = enabled;
		}

		public boolean getAsyncChannel()
		{
			return asyncChannel;
		}

		public void setAsyncChannel(boolean enabled)
		{
			asyncChannel = enabled;
		}

		@Override public int describeContents()
		{
			return 0;
		}

		@Override public void writeToParcel(@NonNull Parcel out, int flags)
		{
			out.writeBoolean(asyncChannel);
			out.writeBoolean(asyncTransport);
			out.writeBoolean(asyncUpdate);
			out.writeString(debug);
		}
	}

	// Session Settings
	public static class AdvancedSettings implements Parcelable
	{
		public static final Parcelable.Creator<AdvancedSettings> CREATOR =
		    new Parcelable.Creator<AdvancedSettings>() {
			    @NonNull public AdvancedSettings createFromParcel(Parcel in)
			    {
				    return new AdvancedSettings(in);
			    }

			    @NonNull @Override public AdvancedSettings[] newArray(int size)
			    {
				    return new AdvancedSettings[size];
			    }
		    };

		@NonNull private String loadBalanceInfo = "";
		private boolean redirectSDCard = false;
		private int redirectSound = 0;
		private boolean redirectMicrophone = false;
		private boolean redirectPrinter = false;
		private int security = 0;
		private boolean consoleMode = false;
		private boolean vmConnectMode = false;
		@NonNull private String vmConnectGuid = "";

		@NonNull private String remoteProgram = "";

		@NonNull private String workDir = "";
		private int tlsSecLevel = -1;
		private int tlsMinLevel = -1;

		public AdvancedSettings()
		{
		}

		public AdvancedSettings(@NonNull Parcel parcel)
		{
			loadBalanceInfo = Objects.requireNonNull(parcel.readString());
			redirectSDCard = parcel.readBoolean();
			redirectSound = parcel.readInt();
			redirectMicrophone = parcel.readBoolean();
			redirectPrinter = parcel.readBoolean();
			security = parcel.readInt();
			consoleMode = parcel.readBoolean();
			vmConnectMode = parcel.readBoolean();
			vmConnectGuid = Objects.requireNonNull(parcel.readString());
			remoteProgram = Objects.requireNonNull(parcel.readString());
			workDir = Objects.requireNonNull(parcel.readString());
			tlsSecLevel = parcel.readInt();
			tlsMinLevel = parcel.readInt();
		}

		private void validate()
		{
			switch (redirectSound)
			{
				case 0:
				case 1:
				case 2:
					break;
				default:
					redirectSound = 0;
					break;
			}

			switch (security)
			{
				case 0:
				case 1:
				case 2:
				case 3:
					break;
				default:
					security = 0;
					break;
			}
		}

		public int getTlsSecLevel()
		{
			return tlsSecLevel;
		}

		public void setTlsSecLevel(int level)
		{
			tlsSecLevel = level;
		}

		public void setTlsMinLevel(int level)
		{
			tlsMinLevel = level;
		}

		public int getTlsMinLevel()
		{
			return tlsMinLevel;
		}

		@NonNull public String getLoadBalanceInfo()
		{
			return loadBalanceInfo;
		}

		public void setLoadBalanceInfo(@NonNull String info)
		{
			loadBalanceInfo = info;
		}
		public boolean getRedirectSDCard()
		{
			return redirectSDCard;
		}

		public void setRedirectSDCard(boolean redirectSDCard)
		{
			this.redirectSDCard = redirectSDCard;
		}

		public boolean getRedirectPrinter()
		{
			return redirectPrinter;
		}

		public void setRedirectPrinter(boolean redirectPrinter)
		{
			this.redirectPrinter = redirectPrinter;
		}

		public int getRedirectSound()
		{
			validate();
			return redirectSound;
		}

		public void setRedirectSound(int redirect)
		{
			this.redirectSound = redirect;
		}

		public boolean getRedirectMicrophone()
		{
			return redirectMicrophone;
		}

		public void setRedirectMicrophone(boolean redirect)
		{
			this.redirectMicrophone = redirect;
		}

		public int getSecurity()
		{
			validate();
			return security;
		}

		public void setSecurity(int security)
		{
			this.security = security;
		}

		public boolean getConsoleMode()
		{
			return consoleMode;
		}

		public void setConsoleMode(boolean consoleMode)
		{
			this.consoleMode = consoleMode;
		}

		@NonNull public String getRemoteProgram()
		{
			return remoteProgram;
		}

		public void setRemoteProgram(@NonNull String remoteProgram)
		{
			this.remoteProgram = remoteProgram;
		}

		@NonNull public String getWorkDir()
		{
			return workDir;
		}

		public void setWorkDir(@NonNull String workDir)
		{
			this.workDir = workDir;
		}

		public boolean getVmConnectMode()
		{
			return vmConnectMode;
		}

		public void setVmConnectMode(boolean vmConnectMode)
		{
			this.vmConnectMode = vmConnectMode;
		}

		@NonNull public String getVmConnectGuid()
		{
			return vmConnectGuid;
		}

		public void setVmConnectGuid(@NonNull String vmConnectGuid)
		{
			this.vmConnectGuid = vmConnectGuid;
		}

		@Override public int describeContents()
		{
			return 0;
		}

		@Override public void writeToParcel(@NonNull Parcel out, int flags)
		{
			out.writeString(loadBalanceInfo);
			out.writeBoolean(redirectSDCard);
			out.writeInt(redirectSound);
			out.writeBoolean(redirectMicrophone);
			out.writeBoolean(redirectPrinter);
			out.writeInt(security);
			out.writeBoolean(consoleMode);
			out.writeBoolean(vmConnectMode);
			out.writeString(vmConnectGuid);
			out.writeString(remoteProgram);
			out.writeString(workDir);
			out.writeInt(tlsSecLevel);
			out.writeInt(tlsMinLevel);
		}
	}

	public static class GatewaySettings implements Parcelable
	{
		public static final Parcelable.Creator<GatewaySettings> CREATOR =
		    new Parcelable.Creator<GatewaySettings>() {
			    @NonNull public GatewaySettings createFromParcel(Parcel in)
			    {
				    return new GatewaySettings(in);
			    }

			    @NonNull @Override public GatewaySettings[] newArray(int size)
			    {
				    return new GatewaySettings[size];
			    }
		    };

		@NonNull private String hostname = "";
		private int port = 443;

		@NonNull private String username = "";

		@NonNull private String password = "";

		@NonNull private String domain = "";

		public GatewaySettings()
		{
		}

		public GatewaySettings(Parcel parcel)
		{
			hostname = Objects.requireNonNull(parcel.readString());
			port = parcel.readInt();
			username = Objects.requireNonNull(parcel.readString());
			password = Objects.requireNonNull(parcel.readString());
			domain = Objects.requireNonNull(parcel.readString());
		}

		@NonNull public String getHostname()
		{
			return hostname;
		}

		public void setHostname(@NonNull String hostname)
		{
			this.hostname = hostname;
		}

		public int getPort()
		{
			return port;
		}

		public void setPort(int port)
		{
			this.port = port;
		}

		@NonNull public String getUsername()
		{
			return username;
		}

		public void setUsername(@NonNull String username)
		{
			this.username = username;
		}

		@NonNull public String getPassword()
		{
			return password;
		}

		public void setPassword(@NonNull String password)
		{
			this.password = password;
		}

		@NonNull public String getDomain()
		{
			return domain;
		}

		public void setDomain(@NonNull String domain)
		{
			this.domain = domain;
		}

		@Override public int describeContents()
		{
			return 0;
		}

		@Override public void writeToParcel(@NonNull Parcel out, int flags)
		{
			out.writeString(hostname);
			out.writeInt(port);
			out.writeString(username);
			out.writeString(password);
			out.writeString(domain);
		}
	}
}
