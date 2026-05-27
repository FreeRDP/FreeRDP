/*
   Android Session Auth Dialogs

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
   Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
   If a copy of the MPL was not distributed with this file, You can obtain one at
   http://mozilla.org/MPL/2.0/.
 */

package com.freerdp.freerdpcore.presentation;

import android.app.Activity;
import android.app.AlertDialog;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.services.LibFreeRDP;

/**
 * Owns every dialog that the FreeRDP session may need: the connect-progress
 * spinner, the credentials prompt, and the certificate verification dialogs.
 * The auth/verify methods block the calling (background) thread until the user
 * dismisses the dialog on the UI thread.
 */
public class SessionDialogs
{
	/** Notified when the user cancels any of the dialogs. */
	public interface OnUserCancelListener
	{
		void onUserCancel();
	}

	/** Notified when the user cancels the connection-progress dialog. */
	public interface OnConnectCancelListener
	{
		void onConnectCancel();
	}

	private final Activity activity;
	private final OnUserCancelListener cancelListener;
	private final Handler mainHandler = new Handler(Looper.getMainLooper());

	private final AlertDialog dlgVerifyCertificate;
	private final AlertDialog dlgUserCredentials;
	private final View userCredView;

	private AlertDialog progressDialog;

	private boolean callbackDialogResult;

	public SessionDialogs(Activity activity, OnUserCancelListener cancelListener)
	{
		this.activity = activity;
		this.cancelListener = cancelListener;

		dlgVerifyCertificate = new AlertDialog.Builder(activity)
		                           .setTitle(R.string.dlg_title_verify_certificate)
		                           .setPositiveButton(android.R.string.yes,
		                                              (dialog, which) -> {
			                                              callbackDialogResult = true;
			                                              synchronized (dialog)
			                                              {
				                                              dialog.notify();
			                                              }
		                                              })
		                           .setNegativeButton(android.R.string.no,
		                                              (dialog, which) -> {
			                                              callbackDialogResult = false;
			                                              notifyCancel();
			                                              synchronized (dialog)
			                                              {
				                                              dialog.notify();
			                                              }
		                                              })
		                           .setCancelable(false)
		                           .create();

		userCredView = activity.getLayoutInflater().inflate(R.layout.credentials, null, true);
		dlgUserCredentials = new AlertDialog.Builder(activity)
		                         .setView(userCredView)
		                         .setTitle(R.string.dlg_title_credentials)
		                         .setPositiveButton(android.R.string.ok,
		                                            (dialog, which) -> {
			                                            callbackDialogResult = true;
			                                            synchronized (dialog)
			                                            {
				                                            dialog.notify();
			                                            }
		                                            })
		                         .setNegativeButton(android.R.string.cancel,
		                                            (dialog, which) -> {
			                                            callbackDialogResult = false;
			                                            notifyCancel();
			                                            synchronized (dialog)
			                                            {
				                                            dialog.notify();
			                                            }
		                                            })
		                         .setCancelable(false)
		                         .create();
	}

	/**
	 * Shows the credentials dialog and blocks until the user dismisses it.
	 * On OK the buffers are overwritten with the entered values.
	 */
	public boolean promptCredentials(StringBuilder username, StringBuilder domain,
	                                 StringBuilder password)
	{
		callbackDialogResult = false;

		((EditText)userCredView.findViewById(R.id.editTextUsername)).setText(username);
		((EditText)userCredView.findViewById(R.id.editTextDomain)).setText(domain);
		((EditText)userCredView.findViewById(R.id.editTextPassword)).setText(password);

		showOnUiThread(dlgUserCredentials);

		try
		{
			synchronized (dlgUserCredentials)
			{
				dlgUserCredentials.wait();
			}
		}
		catch (InterruptedException e)
		{
		}

		username.setLength(0);
		domain.setLength(0);
		password.setLength(0);

		username.append(
		    ((EditText)userCredView.findViewById(R.id.editTextUsername)).getText().toString());
		domain.append(
		    ((EditText)userCredView.findViewById(R.id.editTextDomain)).getText().toString());
		password.append(
		    ((EditText)userCredView.findViewById(R.id.editTextPassword)).getText().toString());

		return callbackDialogResult;
	}

	/**
	 * Shows the certificate verification dialog. Returns 1 if accepted, 0 otherwise.
	 */
	public int verifyCertificate(String host, long port, String subject, String issuer,
	                             String fingerprint, long flags)
	{
		String msg = activity.getResources().getString(R.string.dlg_msg_verify_certificate);
		String type = "RDP-Server";
		if ((flags & LibFreeRDP.VERIFY_CERT_FLAG_GATEWAY) != 0)
			type = "RDP-Gateway";
		if ((flags & LibFreeRDP.VERIFY_CERT_FLAG_REDIRECT) != 0)
			type = "RDP-Redirect";
		msg += "\n\n" + type + ": " + host + ":" + port;
		msg += "\n\nSubject: " + subject + "\nIssuer: " + issuer;
		if ((flags & LibFreeRDP.VERIFY_CERT_FLAG_FP_IS_PEM) != 0)
			msg += "\nCertificate: " + fingerprint;
		else
			msg += "\nFingerprint: " + fingerprint;

		return showVerifyDialog(msg);
	}

	/**
	 * Shows the certificate-changed verification dialog. Returns 1 if accepted, 0 otherwise.
	 */
	public int verifyChangedCertificate(String host, long port, String subject, String issuer,
	                                    String fingerprint, long flags)
	{
		// The "changed" variant currently shows the same information as the
		// regular verify dialog (matches prior behaviour).
		return verifyCertificate(host, port, subject, issuer, fingerprint, flags);
	}

	private int showVerifyDialog(String msg)
	{
		callbackDialogResult = false;
		dlgVerifyCertificate.setMessage(msg);

		showOnUiThread(dlgVerifyCertificate);

		try
		{
			synchronized (dlgVerifyCertificate)
			{
				dlgVerifyCertificate.wait();
			}
		}
		catch (InterruptedException e)
		{
		}

		return callbackDialogResult ? 1 : 0;
	}

	private void showOnUiThread(final AlertDialog dialog)
	{
		mainHandler.post(dialog::show);
	}

	private void notifyCancel()
	{
		if (cancelListener != null)
			cancelListener.onUserCancel();
	}

	/**
	 * Builds and shows a non-blocking connect-progress dialog. Subsequent calls
	 * replace any previously shown progress dialog.
	 */
	public void showProgress(String title, final OnConnectCancelListener listener)
	{
		dismissProgress();

		int pad = (int)TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, 24,
		                                         activity.getResources().getDisplayMetrics());
		LinearLayout content = new LinearLayout(activity);
		content.setOrientation(LinearLayout.HORIZONTAL);
		content.setPadding(pad, pad, pad, pad);
		content.setGravity(Gravity.CENTER_VERTICAL);

		ProgressBar progressBar = new ProgressBar(activity);
		progressBar.setIndeterminate(true);
		content.addView(progressBar,
		                new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT,
		                                              ViewGroup.LayoutParams.WRAP_CONTENT));

		TextView messageView = new TextView(activity);
		messageView.setText(R.string.dlg_msg_connecting);
		LinearLayout.LayoutParams textParams = new LinearLayout.LayoutParams(
		    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
		textParams.leftMargin = pad;
		content.addView(messageView, textParams);

		progressDialog = new AlertDialog.Builder(activity)
		                     .setTitle(title)
		                     .setView(content)
		                     .setNegativeButton(android.R.string.cancel,
		                                        (dialog, which) -> {
			                                        if (listener != null)
				                                        listener.onConnectCancel();
		                                        })
		                     .setCancelable(false)
		                     .create();
		progressDialog.show();
	}

	/** Dismisses the connect-progress dialog if it is currently shown. */
	public void dismissProgress()
	{
		if (progressDialog != null)
		{
			progressDialog.dismiss();
			progressDialog = null;
		}
	}
}
