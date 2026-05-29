/*
   Android Clipboard Manager Proxy

   Copyright 2013 Thincast Technologies GmbH
   Copyright 2013 Martin Fleisz <martin.fleisz@thincast.com>
   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
 */

package com.freerdp.freerdpcore.utils;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.net.Uri;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

public abstract class ClipboardManagerProxy
{
	public static ClipboardManagerProxy getClipboardManager(Context ctx)
	{
		return new HCClipboardManager(ctx);
	}

	public abstract void setClipboardData(String data);

	public abstract void setClipboardImage(byte[] pngData);

	public abstract void addClipboardChangedListener(OnClipboardChangedListener listener);

	public abstract void removeClipboardboardChangedListener(OnClipboardChangedListener listener);

	public abstract void getPrimaryClipManually();

	public interface OnClipboardChangedListener
	{
		void onClipboardChanged(String data);

		void onClipboardImageChanged(byte[] data, String mimeType);
	}

	private static class HCClipboardManager
	    extends ClipboardManagerProxy implements ClipboardManager.OnPrimaryClipChangedListener
	{
		private final Context mContext;
		private final ClipboardManager mClipboardManager;
		private OnClipboardChangedListener mListener;

		public HCClipboardManager(Context ctx)
		{
			mContext = ctx;
			mClipboardManager = (ClipboardManager)ctx.getSystemService(Context.CLIPBOARD_SERVICE);
		}

		@Override public void setClipboardData(String data)
		{
			mClipboardManager.setPrimaryClip(
			    ClipData.newPlainText("rdp-clipboard", data == null ? "" : data));
		}

		@Override public void setClipboardImage(byte[] pngData)
		{
			ClipboardImageProvider.setImageData(pngData);
			ClipData clip = new ClipData("rdp-clipboard", new String[] { "image/png" },
			                             new ClipData.Item(ClipboardImageProvider.CONTENT_URI));
			mClipboardManager.setPrimaryClip(clip);
		}

		@Override public void onPrimaryClipChanged()
		{
			ClipData clip = mClipboardManager.getPrimaryClip();
			if (clip == null || clip.getItemCount() == 0)
				return;

			ClipData.Item item = clip.getItemAt(0);

			CharSequence cs = item.getText();
			if (cs != null)
			{
				if (mListener != null)
					mListener.onClipboardChanged(cs.toString());
				return;
			}

			Uri uri = item.getUri();
			if (uri != null)
			{
				String mimeType = mContext.getContentResolver().getType(uri);
				if (mimeType != null && mimeType.startsWith("image/"))
				{
					try (InputStream is = mContext.getContentResolver().openInputStream(uri))
					{
						if (is != null)
						{
							ByteArrayOutputStream baos = new ByteArrayOutputStream();
							byte[] buf = new byte[8192];
							int n;
							while ((n = is.read(buf)) != -1)
								baos.write(buf, 0, n);
							if (mListener != null)
								mListener.onClipboardImageChanged(baos.toByteArray(), mimeType);
						}
					}
					catch (IOException e)
					{
						// not an accessible image
					}
				}
			}
		}

		@Override public void addClipboardChangedListener(OnClipboardChangedListener listener)
		{
			mListener = listener;
			mClipboardManager.addPrimaryClipChangedListener(this);
		}

		@Override
		public void removeClipboardboardChangedListener(OnClipboardChangedListener listener)
		{
			mListener = null;
			mClipboardManager.removePrimaryClipChangedListener(this);
		}

		@Override public void getPrimaryClipManually()
		{
			onPrimaryClipChanged();
		}
	}
}
