/* Bookmark entity */

package com.freerdp.freerdpcore.data;

import androidx.room.ColumnInfo;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "bookmarks") public class BookmarkEntity
{
	@PrimaryKey(autoGenerate = true) @ColumnInfo(name = "id") public long id;

	@ColumnInfo(name = "label") public String label = "";

	@ColumnInfo(name = "hostname") public String hostname = "";

	@ColumnInfo(name = "username") public String username = "";

	@ColumnInfo(name = "password") public String password = "";

	@ColumnInfo(name = "domain") public String domain = "";

	@ColumnInfo(name = "port") public int port = 3389;

	@ColumnInfo(name = "colors") public int colors = 32;

	/** FITSCREEN=-2, AUTOMATIC=-1, CUSTOM=0, PREDEFINED=1 */
	@ColumnInfo(name = "resolution") public int resolution = -1;

	@ColumnInfo(name = "width") public int width = 0;

	@ColumnInfo(name = "height") public int height = 0;

	@ColumnInfo(name = "perf_remotefx") public boolean perfRemoteFx = true;

	@ColumnInfo(name = "perf_gfx") public boolean perfGfx = true;

	@ColumnInfo(name = "perf_gfx_h264") public boolean perfGfxH264 = true;

	@ColumnInfo(name = "perf_wallpaper") public boolean perfWallpaper = true;

	@ColumnInfo(name = "perf_theming") public boolean perfTheming = true;

	@ColumnInfo(name = "perf_full_window_drag") public boolean perfFullWindowDrag = true;

	@ColumnInfo(name = "perf_menu_animations") public boolean perfMenuAnimations = true;

	@ColumnInfo(name = "perf_font_smoothing") public boolean perfFontSmoothing = true;

	@ColumnInfo(name = "perf_desktop_composition") public boolean perfDesktopComposition = true;

	@ColumnInfo(name = "enable_gateway_settings") public boolean enableGatewaySettings = false;

	@ColumnInfo(name = "gateway_hostname") public String gatewayHostname = "";

	@ColumnInfo(name = "gateway_port") public int gatewayPort = 443;

	@ColumnInfo(name = "gateway_username") public String gatewayUsername = "";

	@ColumnInfo(name = "gateway_password") public String gatewayPassword = "";

	@ColumnInfo(name = "gateway_domain") public String gatewayDomain = "";

	@ColumnInfo(name = "loadbalanceinfo") public String loadbalanceinfo = "";

	@ColumnInfo(name = "redirect_sdcard") public boolean redirectSdcard = false;

	/** 0=off, 1=local, 2=remote */
	@ColumnInfo(name = "redirect_sound") public int redirectSound = 0;

	@ColumnInfo(name = "redirect_microphone") public boolean redirectMicrophone = false;

	/** 0=auto, 1=rdp, 2=tls, 3=nla */
	@ColumnInfo(name = "security") public int security = 0;

	@ColumnInfo(name = "remote_program") public String remoteProgram = "";

	@ColumnInfo(name = "work_dir") public String workDir = "";

	@ColumnInfo(name = "console_mode") public boolean consoleMode = false;

	@ColumnInfo(name = "debug_level") public String debugLevel = "INFO";

	@ColumnInfo(name = "async_channel") public boolean asyncChannel = false;

	@ColumnInfo(name = "async_update") public boolean asyncUpdate = false;

	@ColumnInfo(name = "tlsSecLevel", defaultValue = "-1") public int tlsSecLevel = -1;

	@ColumnInfo(name = "tlsMinLevel", defaultValue = "-1") public int tlsMinLevel = -1;
}
