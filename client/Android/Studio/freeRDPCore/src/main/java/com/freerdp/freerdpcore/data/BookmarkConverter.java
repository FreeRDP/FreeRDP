/*
   Bookmark entity/domain model converter

   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.data;

import com.freerdp.freerdpcore.domain.BookmarkBase;

public final class BookmarkConverter
{
	private BookmarkConverter()
	{
	}

	public static BookmarkBase toBookmark(BookmarkEntity e)
	{
		BookmarkBase bm = new BookmarkBase();

		bm.setId(e.id);
		bm.setLabel(e.label);
		bm.setUsername(e.username);
		bm.setPassword(e.password);
		bm.setDomain(e.domain);
		bm.setHostname(e.hostname);
		bm.setPort(e.port);

		BookmarkBase.ScreenSettings screen = bm.getScreenSettings();
		screen.setColors(e.colors);
		screen.setResolution(e.resolution);
		screen.setWidth(e.width);
		screen.setHeight(e.height);
		screen.setScale(e.scaleMode, e.scaleDesktop, e.scaleDevice);

		BookmarkBase.PerformanceFlags perf = bm.getPerformanceFlags();
		perf.setRemoteFX(e.perfRemoteFx);
		perf.setGfx(e.perfGfx);
		perf.setH264(e.perfGfxH264);
		perf.setWallpaper(e.perfWallpaper);
		perf.setTheming(e.perfTheming);
		perf.setFullWindowDrag(e.perfFullWindowDrag);
		perf.setMenuAnimations(e.perfMenuAnimations);
		perf.setFontSmoothing(e.perfFontSmoothing);
		perf.setDesktopComposition(e.perfDesktopComposition);

		BookmarkBase.AdvancedSettings adv = bm.getAdvancedSettings();
		adv.setLoadBalanceInfo(e.loadbalanceinfo);
		adv.setRedirectSDCard(e.redirectSdcard);
		adv.setRedirectSound(e.redirectSound);
		adv.setRedirectMicrophone(e.redirectMicrophone);
		adv.setRedirectPrinter(e.redirectPrinter);
		adv.setSecurity(e.security);
		adv.setRemoteProgram(e.remoteProgram);
		adv.setWorkDir(e.workDir);
		adv.setConsoleMode(e.consoleMode);
		adv.setTlsSecLevel(e.tlsSecLevel);
		adv.setTlsMinLevel(e.tlsMinLevel);
		adv.setVmConnectMode(e.vmConnectMode);
		adv.setVmConnectGuid(e.vmConnectGuid);

		bm.setEnableGatewaySettings(e.enableGatewaySettings);
		BookmarkBase.GatewaySettings gw = bm.getGatewaySettings();
		gw.setHostname(e.gatewayHostname);
		gw.setPort(e.gatewayPort);
		gw.setUsername(e.gatewayUsername);
		gw.setPassword(e.gatewayPassword);
		gw.setDomain(e.gatewayDomain);

		BookmarkBase.DebugSettings dbg = bm.getDebugSettings();
		dbg.setDebugLevel(e.debugLevel);
		dbg.setAsyncChannel(e.asyncChannel);
		dbg.setAsyncUpdate(e.asyncUpdate);

		return bm;
	}

	public static BookmarkEntity toEntity(BookmarkBase bm)
	{
		BookmarkEntity e = new BookmarkEntity();
		if (bm.getId() > 0)
		{
			e.id = bm.getId();
		}
		e.label = bm.getLabel();
		e.username = bm.getUsername();
		e.password = bm.getPassword();
		e.domain = bm.getDomain();
		e.hostname = bm.getHostname();
		e.port = bm.getPort();

		BookmarkBase.ScreenSettings screen = bm.getScreenSettings();
		e.colors = screen.getColors();
		e.resolution = screen.getResolution();
		e.width = screen.getWidth();
		e.height = screen.getHeight();
		e.scaleMode = screen.getScaleMode();
		e.scaleDesktop = screen.getScaleDesktop();
		e.scaleDevice = screen.getScaleDevice();

		BookmarkBase.PerformanceFlags perf = bm.getPerformanceFlags();
		e.perfRemoteFx = perf.getRemoteFX();
		e.perfGfx = perf.getGfx();
		e.perfGfxH264 = perf.getH264();
		e.perfWallpaper = perf.getWallpaper();
		e.perfTheming = perf.getTheming();
		e.perfFullWindowDrag = perf.getFullWindowDrag();
		e.perfMenuAnimations = perf.getMenuAnimations();
		e.perfFontSmoothing = perf.getFontSmoothing();
		e.perfDesktopComposition = perf.getDesktopComposition();

		BookmarkBase.AdvancedSettings adv = bm.getAdvancedSettings();
		e.loadbalanceinfo = adv.getLoadBalanceInfo();
		e.redirectSdcard = adv.getRedirectSDCard();
		e.redirectSound = adv.getRedirectSound();
		e.redirectMicrophone = adv.getRedirectMicrophone();
		e.redirectPrinter = adv.getRedirectPrinter();
		e.security = adv.getSecurity();
		e.remoteProgram = adv.getRemoteProgram();
		e.workDir = adv.getWorkDir();
		e.consoleMode = adv.getConsoleMode();
		e.tlsSecLevel = adv.getTlsSecLevel();
		e.tlsMinLevel = adv.getTlsMinLevel();
		e.vmConnectMode = adv.getVmConnectMode();
		e.vmConnectGuid = adv.getVmConnectGuid();

		e.enableGatewaySettings = bm.getEnableGatewaySettings();
		BookmarkBase.GatewaySettings gw = bm.getGatewaySettings();
		e.gatewayHostname = gw.getHostname();
		e.gatewayPort = gw.getPort();
		e.gatewayUsername = gw.getUsername();
		e.gatewayPassword = gw.getPassword();
		e.gatewayDomain = gw.getDomain();

		BookmarkBase.DebugSettings dbg = bm.getDebugSettings();
		e.debugLevel = dbg.getDebugLevel();
		e.asyncChannel = dbg.getAsyncChannel();
		e.asyncUpdate = dbg.getAsyncUpdate();

		return e;
	}
}
