/*
   Android Shortcut activity

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.presentation;

import java.util.ArrayList;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.application.GlobalApp;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.services.SessionRequestHandlerActivity;
import com.freerdp.freerdpcore.utils.BookmarkArrayAdapter;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Parcelable;
import android.view.View;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.AdapterView;

public class ShortcutsActivity extends ListActivity {

	public static final String TAG = "ShortcutsActivity";
	
	
	@Override
	public void onCreate(Bundle savedInstanceState) {

		super.onCreate(savedInstanceState);
		
		Intent intent = getIntent();
		if(Intent.ACTION_CREATE_SHORTCUT.equals(intent.getAction()))
		{				
			// set listeners for the list view
			getListView().setOnItemClickListener(new AdapterView.OnItemClickListener() {
				public void onItemClick(AdapterView<?> parent, View view, int position, long id)
				{
					String refStr = view.getTag().toString();
					String defLabel = ((TextView)(view.findViewById(R.id.bookmark_text1))).getText().toString();
					setupShortcut(refStr, defLabel);
				}
			});			
		}
		else
		{
			// just exit
			finish();
		}
	}
	
	@Override
	public void onResume() {
		super.onResume();
		// create bookmark cursor adapter
		ArrayList<BookmarkBase> bookmarks = GlobalApp.getManualBookmarkGateway().findAll();
		BookmarkArrayAdapter bookmarkAdapter = new BookmarkArrayAdapter(this, android.R.layout.simple_list_item_2, bookmarks);
		getListView().setAdapter(bookmarkAdapter);					
	}
	
	public void onPause() {
		super.onPause();
		getListView().setAdapter(null);
	}
	
    /**
     * This function creates a shortcut and returns it to the caller.  There are actually two 
     * intents that you will send back.
     * 
     * The first intent serves as a container for the shortcut and is returned to the launcher by 
     * setResult().  This intent must contain three fields:
     * 
     * <ul>
     * <li>{@link android.content.Intent#EXTRA_SHORTCUT_INTENT} The shortcut intent.</li>
     * <li>{@link android.content.Intent#EXTRA_SHORTCUT_NAME} The text that will be displayed with
     * the shortcut.</li>
     * <li>{@link android.content.Intent#EXTRA_SHORTCUT_ICON} The shortcut's icon, if provided as a
     * bitmap, <i>or</i> {@link android.content.Intent#EXTRA_SHORTCUT_ICON_RESOURCE} if provided as
     * a drawable resource.</li>
     * </ul>
     * 
     * If you use a simple drawable resource, note that you must wrapper it using
     * {@link android.content.Intent.ShortcutIconResource}, as shown below.  This is required so
     * that the launcher can access resources that are stored in your application's .apk file.  If 
     * you return a bitmap, such as a thumbnail, you can simply put the bitmap into the extras 
     * bundle using {@link android.content.Intent#EXTRA_SHORTCUT_ICON}.
     * 
     * The shortcut intent can be any intent that you wish the launcher to send, when the user 
     * clicks on the shortcut.  Typically this will be {@link android.content.Intent#ACTION_VIEW} 
     * with an appropriate Uri for your content, but any Intent will work here as long as it 
     * triggers the desired action within your Activity.
     */
	
	private void setupShortcut(String strRef, String defaultLabel) {
		final String paramStrRef = strRef;
		final String paramDefaultLabel = defaultLabel;
		final Context paramContext = this;	
    	
		// display edit dialog to the user so he can specify the shortcut name
		final EditText input = new EditText(this);
		input.setText(defaultLabel);
		    	
    	AlertDialog.Builder builder = new AlertDialog.Builder(this);
    	builder.setTitle(R.string.dlg_title_create_shortcut)
    		   .setMessage(R.string.dlg_msg_create_shortcut)
		       .setView(input)
    		   .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() 
    		   {
    				@Override
    		 	   	public void onClick(DialogInterface dialog, int which) 
    				{
    					String label = input.getText().toString();
    					if(label.length() == 0)
    						label = paramDefaultLabel;
    					
    		 	        Intent shortcutIntent = new Intent(Intent.ACTION_VIEW);
    		 	        shortcutIntent.setClassName(paramContext, SessionRequestHandlerActivity.class.getName());
    		 	        shortcutIntent.setData(Uri.parse(paramStrRef));

    		 	        // Then, set up the container intent (the response to the caller)
    		 	        Intent intent = new Intent();
    		 	        intent.putExtra(Intent.EXTRA_SHORTCUT_INTENT, shortcutIntent);
    		 	        intent.putExtra(Intent.EXTRA_SHORTCUT_NAME, label);
    		 	        Parcelable iconResource = Intent.ShortcutIconResource.fromContext(paramContext, R.drawable.icon_launcher_freerdp);
    		 	        intent.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE, iconResource);

    		 	        // Now, return the result to the launcher
    		 	        setResult(RESULT_OK, intent);
    		 	        finish();
    		 	   }
   		 	   })
		       .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() 
		       {				
					@Override
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
					}
		       })
    		   .create().show();
    	    	
    }
	
}
