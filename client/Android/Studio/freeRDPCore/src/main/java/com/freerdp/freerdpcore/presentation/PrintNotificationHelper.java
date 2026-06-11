/*
   Print Notification Helper

   Copyright 2026 Ibrahim Sevinc

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.core.app.NotificationCompat;
import androidx.core.content.FileProvider;

import com.freerdp.freerdpcore.R;

import java.io.File;
import java.util.concurrent.atomic.AtomicInteger;

public class PrintNotificationHelper
{
	private static final String CHANNEL_ID = "rdp_print";
	private static final String AUTHORITY = "com.freerdp.afreerdp.fileprovider";
	private static final AtomicInteger notifId = new AtomicInteger(1000);

	public static void ensureChannel(Context ctx)
	{
		NotificationManager nm = ctx.getSystemService(NotificationManager.class);
		if (nm.getNotificationChannel(CHANNEL_ID) != null)
			return;
		NotificationChannel ch =
		    new NotificationChannel(CHANNEL_ID, ctx.getString(R.string.print_notif_channel),
		                            NotificationManager.IMPORTANCE_HIGH);
		nm.createNotificationChannel(ch);
	}

	public static void notify(Context ctx, File pdfFile)
	{
		ensureChannel(ctx);

		Uri uri = FileProvider.getUriForFile(ctx, AUTHORITY, pdfFile);

		PendingIntent openIntent = PendingIntent.getActivity(
		    ctx, notifId.get(),
		    new Intent(Intent.ACTION_VIEW)
		        .setDataAndType(uri, "application/pdf")
		        .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION),
		    PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

		Intent printLaunch = new Intent(ctx, PrintProxyActivity.class);
		printLaunch.setData(uri);
		printLaunch.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_GRANT_READ_URI_PERMISSION);
		PendingIntent printIntent = PendingIntent.getActivity(
		    ctx, notifId.get() + 1, printLaunch,
		    PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT);

		Notification notif = new NotificationCompat.Builder(ctx, CHANNEL_ID)
		                         .setSmallIcon(android.R.drawable.ic_dialog_info)
		                         .setContentTitle(ctx.getString(R.string.print_notif_title))
		                         .setContentText(pdfFile.getName())
		                         .setContentIntent(openIntent)
		                         .addAction(android.R.drawable.ic_menu_view,
		                                    ctx.getString(R.string.print_notif_open), openIntent)
		                         .addAction(android.R.drawable.ic_menu_send,
		                                    ctx.getString(R.string.print_notif_print), printIntent)
		                         .setAutoCancel(true)
		                         .build();

		NotificationManager nm =
		    (NotificationManager)ctx.getSystemService(Context.NOTIFICATION_SERVICE);
		nm.notify(notifId.getAndIncrement(), notif);
	}
}
