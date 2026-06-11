/*
   Bookmark entity

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import androidx.annotation.NonNull;
import androidx.room.ColumnInfo;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "bookmarks") public class BookmarkEntity
{
	@PrimaryKey(autoGenerate = true) @ColumnInfo(name = "id") public long id;

	@NonNull @ColumnInfo(name = "label", defaultValue = "") public String label = "";

	@NonNull @ColumnInfo(name = "hostname", defaultValue = "") public String hostname = "";

	@NonNull @ColumnInfo(name = "username", defaultValue = "") public String username = "";

	@NonNull @ColumnInfo(name = "password", defaultValue = "") public String password = "";

	@NonNull @ColumnInfo(name = "domain", defaultValue = "") public String domain = "";

	@ColumnInfo(name = "port", defaultValue = "3389") public int port = 3389;

	@ColumnInfo(name = "colors", defaultValue = "32") public int colors = 32;

	/** FITSCREEN=-2, AUTOMATIC=-1, CUSTOM=0, PREDEFINED=1 */
	@ColumnInfo(name = "resolution", defaultValue = "-1") public int resolution = -1;

	@ColumnInfo(name = "width", defaultValue = "0") public int width = 0;

	@ColumnInfo(name = "height", defaultValue = "0") public int height = 0;

	@NonNull @ColumnInfo(name = "scale_mode", defaultValue = "100") public String scaleMode = "100";

	@ColumnInfo(name = "scale_desktop", defaultValue = "100") public int scaleDesktop = 100;

	@ColumnInfo(name = "scale_device", defaultValue = "100") public int scaleDevice = 100;

	@ColumnInfo(name = "perf_remotefx", defaultValue = "true") public boolean perfRemoteFx = true;

	@ColumnInfo(name = "perf_gfx", defaultValue = "true") public boolean perfGfx = true;

	@ColumnInfo(name = "perf_gfx_h264", defaultValue = "true") public boolean perfGfxH264 = true;

	@ColumnInfo(name = "perf_wallpaper", defaultValue = "true") public boolean perfWallpaper = true;

	@ColumnInfo(name = "perf_theming", defaultValue = "true") public boolean perfTheming = true;

	@ColumnInfo(name = "perf_full_window_drag", defaultValue = "true")
	public boolean perfFullWindowDrag = true;

	@ColumnInfo(name = "perf_menu_animations", defaultValue = "true")
	public boolean perfMenuAnimations = true;

	@ColumnInfo(name = "perf_font_smoothing", defaultValue = "true")
	public boolean perfFontSmoothing = true;

	@ColumnInfo(name = "perf_desktop_composition", defaultValue = "true")
	public boolean perfDesktopComposition = true;

	@ColumnInfo(name = "enable_gateway_settings", defaultValue = "false")
	public boolean enableGatewaySettings = false;

	@NonNull
	@ColumnInfo(name = "gateway_hostname", defaultValue = "")
	public String gatewayHostname = "";

	@ColumnInfo(name = "gateway_port", defaultValue = "443") public int gatewayPort = 443;

	@NonNull
	@ColumnInfo(name = "gateway_username", defaultValue = "")
	public String gatewayUsername = "";

	@NonNull
	@ColumnInfo(name = "gateway_password", defaultValue = "")
	public String gatewayPassword = "";

	@NonNull
	@ColumnInfo(name = "gateway_domain", defaultValue = "")
	public String gatewayDomain = "";

	@NonNull
	@ColumnInfo(name = "loadbalanceinfo", defaultValue = "")
	public String loadbalanceinfo = "";

	@ColumnInfo(name = "redirect_sdcard", defaultValue = "false")
	public boolean redirectSdcard = false;

	/** 0=off, 1=local, 2=remote */
	@ColumnInfo(name = "redirect_sound", defaultValue = "0") public int redirectSound = 0;

	@ColumnInfo(name = "redirect_microphone", defaultValue = "false")
	public boolean redirectMicrophone = false;

	@ColumnInfo(name = "redirect_printer", defaultValue = "false")
	public boolean redirectPrinter = false;

	/** 0=auto, 1=rdp, 2=tls, 3=nla */
	@ColumnInfo(name = "security", defaultValue = "0") public int security = 0;

	@NonNull
	@ColumnInfo(name = "remote_program", defaultValue = "")
	public String remoteProgram = "";

	@NonNull @ColumnInfo(name = "work_dir", defaultValue = "") public String workDir = "";

	@ColumnInfo(name = "console_mode", defaultValue = "false") public boolean consoleMode = false;

	@NonNull
	@ColumnInfo(name = "debug_level", defaultValue = "INFO")
	public String debugLevel = "INFO";

	@ColumnInfo(name = "async_channel", defaultValue = "false") public boolean asyncChannel = false;

	@ColumnInfo(name = "async_update", defaultValue = "false") public boolean asyncUpdate = false;

	@ColumnInfo(name = "tlsSecLevel", defaultValue = "-1") public int tlsSecLevel = -1;

	@ColumnInfo(name = "tlsMinLevel", defaultValue = "-1") public int tlsMinLevel = -1;

	@ColumnInfo(name = "vmconnect_mode", defaultValue = "false")
	public boolean vmConnectMode = false;

	@NonNull
	@ColumnInfo(name = "vmconnect_guid", defaultValue = "")
	public String vmConnectGuid = "";
}
