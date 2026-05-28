/*
   RDP file import/export helper

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import android.util.Log;

import com.freerdp.freerdpcore.domain.BookmarkBase;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.StringReader;
import java.nio.charset.StandardCharsets;

public final class RDPFileHelper
{
	private static final String TAG = "RDPFileHelper";

	private RDPFileHelper()
	{
	}

	public static void importFrom(InputStream in, BookmarkBase bookmark) throws IOException
	{
		if (in == null)
			throw new IOException("null stream");

		byte[] content;
		try
		{
			ByteArrayOutputStream buf = new ByteArrayOutputStream(4096);
			byte[] chunk = new byte[4096];
			int n;
			while ((n = in.read(chunk)) != -1)
				buf.write(chunk, 0, n);
			content = buf.toByteArray();
		}
		finally
		{
			in.close();
		}

		String text;
		int b0 = content.length > 0 ? (content[0] & 0xFF) : -1;
		int b1 = content.length > 1 ? (content[1] & 0xFF) : -1;
		if (b0 == 0xFF && b1 == 0xFE)
			text = new String(content, 2, content.length - 2, StandardCharsets.UTF_16LE);
		else if (b0 == 0xFE && b1 == 0xFF)
			text = new String(content, 2, content.length - 2, StandardCharsets.UTF_16BE);
		else
		{
			text = new String(content, StandardCharsets.UTF_8);
			if (text.startsWith("\uFEFF"))
				text = text.substring(1);
		}

		RDPFileParser p = new RDPFileParser();
		p.parse(new StringReader(text));

		String s = p.getString("full address");
		if (s != null)
		{
			int colonIdx = s.lastIndexOf(":");
			if (colonIdx > s.lastIndexOf("]"))
			{
				try
				{
					bookmark.setPort(Integer.parseInt(s.substring(colonIdx + 1)));
				}
				catch (NumberFormatException e)
				{
					Log.e(TAG, "Malformed address");
				}
				s = s.substring(0, colonIdx);
			}
			if (s.startsWith("[") && s.endsWith("]"))
				s = s.substring(1, s.length() - 1);
			bookmark.setHostname(s);
		}

		Integer i = p.getInteger("server port");
		if (i != null)
			bookmark.setPort(i);

		s = p.getString("username");
		if (s != null)
			bookmark.setUsername(s);

		s = p.getString("domain");
		if (s != null)
			bookmark.setDomain(s);

		BookmarkBase.ScreenSettings screen = bookmark.getActiveScreenSettings();
		Integer w = p.getInteger("desktopwidth");
		Integer h = p.getInteger("desktopheight");
		if (w != null && h != null && w > 0 && h > 0)
		{
			screen.setWidth(w);
			screen.setHeight(h);
			screen.setResolution(BookmarkBase.ScreenSettings.CUSTOM);
		}
		i = p.getInteger("session bpp");
		if (i != null)
			screen.setColors(i);

		BookmarkBase.PerformanceFlags perf = bookmark.getPerformanceFlags();
		i = p.getInteger("disable wallpaper");
		if (i != null)
			perf.setWallpaper(i == 0);
		i = p.getInteger("allow font smoothing");
		if (i != null)
			perf.setFontSmoothing(i == 1);
		i = p.getInteger("allow desktop composition");
		if (i != null)
			perf.setDesktopComposition(i == 1);
		i = p.getInteger("disable menu anims");
		if (i != null)
			perf.setMenuAnimations(i == 0);
		i = p.getInteger("disable themes");
		if (i != null)
			perf.setTheming(i == 0);
		i = p.getInteger("disable full window drag");
		if (i != null)
			perf.setFullWindowDrag(i == 0);

		BookmarkBase.AdvancedSettings advanced = bookmark.getAdvancedSettings();
		i = p.getInteger("connect to console");
		if (i != null)
			advanced.setConsoleMode(i == 1);

		i = p.getInteger("audiomode");
		if (i != null && i >= 0 && i <= 2)
			advanced.setRedirectSound(i);
		i = p.getInteger("audiocapturemode");
		if (i != null)
			advanced.setRedirectMicrophone(i == 1);

		s = p.getString("loadbalanceinfo");
		if (s != null && !s.isEmpty())
			advanced.setLoadBalanceInfo(s);

		s = p.getString("remoteapplicationprogram");
		if (s == null || s.isEmpty())
			s = p.getString("alternate shell");
		if (s != null && !s.isEmpty())
			advanced.setRemoteProgram(s);

		s = p.getString("shell working directory");
		if (s != null && !s.isEmpty())
			advanced.setWorkDir(s);

		s = p.getString("gatewayhostname");
		if (s != null && !s.isEmpty())
		{
			String gwHost = s;
			int gwPort = 443;
			int colonIdx = s.lastIndexOf(":");
			if (colonIdx > 0)
			{
				try
				{
					gwPort = Integer.parseInt(s.substring(colonIdx + 1));
					gwHost = s.substring(0, colonIdx);
				}
				catch (NumberFormatException e)
				{
					Log.e(TAG, "Malformed gateway port");
				}
			}
			bookmark.getGatewaySettings().setHostname(gwHost);
			bookmark.getGatewaySettings().setPort(gwPort);
			bookmark.setEnableGatewaySettings(true);
		}
		i = p.getInteger("gatewayusagemethod");
		if (i != null)
			bookmark.setEnableGatewaySettings(i == 1 || i == 2);

		i = p.getInteger("redirectprinters");
		if (i != null)
			advanced.setRedirectPrinter(i == 1);
		i = p.getInteger("disableprinterredirection");
		if (i != null)
			advanced.setRedirectPrinter(i == 0);

		s = p.getString("pcb");
		if (s != null && !s.isEmpty())
		{
			advanced.setVmConnectMode(true);
			advanced.setVmConnectGuid(s);
		}
	}

	public static String toRdpString(BookmarkBase bookmark)
	{
		StringBuilder sb = new StringBuilder();
		BookmarkBase.ScreenSettings screen = bookmark.getActiveScreenSettings();
		BookmarkBase.PerformanceFlags perf = bookmark.getActivePerformanceFlags();
		BookmarkBase.AdvancedSettings adv = bookmark.getAdvancedSettings();

		String host = bookmark.getHostname();
		int port = bookmark.getPort();
		writeString(sb, "full address", port != 3389 ? host + ":" + port : host);

		String username = bookmark.getUsername();
		if (!username.isEmpty())
			writeString(sb, "username", username);

		String domain = bookmark.getDomain();
		if (!domain.isEmpty())
			writeString(sb, "domain", domain);

		if (screen.getResolution() == BookmarkBase.ScreenSettings.CUSTOM)
		{
			writeInt(sb, "desktopwidth", screen.getWidth());
			writeInt(sb, "desktopheight", screen.getHeight());
		}
		writeInt(sb, "session bpp", screen.getColors());

		writeInt(sb, "disable wallpaper", perf.getWallpaper() ? 0 : 1);
		writeInt(sb, "allow font smoothing", perf.getFontSmoothing() ? 1 : 0);
		writeInt(sb, "allow desktop composition", perf.getDesktopComposition() ? 1 : 0);
		writeInt(sb, "disable menu anims", perf.getMenuAnimations() ? 0 : 1);
		writeInt(sb, "disable themes", perf.getTheming() ? 0 : 1);
		writeInt(sb, "disable full window drag", perf.getFullWindowDrag() ? 0 : 1);

		if (adv.getConsoleMode())
			writeInt(sb, "connect to console", 1);

		int audioMode = adv.getRedirectSound();
		if (audioMode != 0)
			writeInt(sb, "audiomode", audioMode);

		if (adv.getRedirectMicrophone())
			writeInt(sb, "audiocapturemode", 1);

		String lbInfo = adv.getLoadBalanceInfo();
		if (!lbInfo.isEmpty())
			writeString(sb, "loadbalanceinfo", lbInfo);

		String remoteApp = adv.getRemoteProgram();
		if (!remoteApp.isEmpty())
			writeString(sb, "remoteapplicationprogram", remoteApp);

		String workDir = adv.getWorkDir();
		if (!workDir.isEmpty())
			writeString(sb, "shell working directory", workDir);

		if (bookmark.getEnableGatewaySettings())
		{
			BookmarkBase.GatewaySettings gw = bookmark.getGatewaySettings();
			String gwHost = gw.getHostname();
			int gwPort = gw.getPort();
			writeString(sb, "gatewayhostname", gwPort != 443 ? gwHost + ":" + gwPort : gwHost);
			writeInt(sb, "gatewayusagemethod", 2);
		}

		if (adv.getVmConnectMode())
			writeString(sb, "pcb", adv.getVmConnectGuid());

		if (adv.getRedirectPrinter())
			writeInt(sb, "redirectprinters", 1);

		return sb.toString();
	}

	private static void writeString(StringBuilder sb, String key, String value)
	{
		sb.append(key).append(":s:").append(value).append("\r\n");
	}

	private static void writeInt(StringBuilder sb, String key, int value)
	{
		sb.append(key).append(":i:").append(value).append("\r\n");
	}
}
