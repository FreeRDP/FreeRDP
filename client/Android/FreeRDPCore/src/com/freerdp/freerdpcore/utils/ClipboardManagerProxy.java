package com.freerdp.freerdpcore.utils;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;

public abstract class ClipboardManagerProxy {	

	public static ClipboardManagerProxy getClipboardManager(Context ctx) {
		if (VERSION.SDK_INT < VERSION_CODES.HONEYCOMB)
			return new PreHCClipboardManager(ctx);
		else
			return new HCClipboardManager(ctx);
	}

	public static interface OnClipboardChangedListener {
		void onClipboardChanged(String data);
	}
	
	public abstract void setClipboardData(String data);
	public abstract void addClipboardChangedListener(OnClipboardChangedListener listener);
	public abstract void removeClipboardboardChangedListener(OnClipboardChangedListener listener);
	
	private static class PreHCClipboardManager extends ClipboardManagerProxy {

		public PreHCClipboardManager(Context ctx) {
			
		}
		
		@Override
		public void setClipboardData(String data) {			
		}

		@Override
		public void addClipboardChangedListener(
				OnClipboardChangedListener listener) {
		}

		@Override
		public void removeClipboardboardChangedListener(
				OnClipboardChangedListener listener) {
		}		
	}

	@TargetApi(11)
	private static class HCClipboardManager extends ClipboardManagerProxy implements ClipboardManager.OnPrimaryClipChangedListener {
		private ClipboardManager mClipboardManager;
		private OnClipboardChangedListener mListener;
		
		public HCClipboardManager(Context ctx) {
			mClipboardManager = (ClipboardManager) ctx.getSystemService(Context.CLIPBOARD_SERVICE);			
		}
		
		@Override
		public void setClipboardData(String data) {
			mClipboardManager.setPrimaryClip(ClipData.newPlainText("rdp-clipboard", data == null ? "" : data));			
		}

		@Override
		public void onPrimaryClipChanged() {
			ClipData clip = mClipboardManager.getPrimaryClip();
			String data = null;
			
			if (clip != null && clip.getItemCount() > 0) {
				CharSequence cs = clip.getItemAt(0).getText();
				if (cs != null)
					data = cs.toString();
			}
			if (mListener != null) {
				mListener.onClipboardChanged(data);
			}
		}
		
		@Override
		public void addClipboardChangedListener(
				OnClipboardChangedListener listener) {
			mListener = listener;
			mClipboardManager.addPrimaryClipChangedListener(this);
		}

		@Override
		public void removeClipboardboardChangedListener(
				OnClipboardChangedListener listener) {
			mListener = null;
			mClipboardManager.removePrimaryClipChangedListener(this);
		}
	}
}
