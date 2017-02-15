/*
   ArrayAdapter for bookmark lists

   Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import java.util.List;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.freerdp.freerdpcore.R;
import com.freerdp.freerdpcore.domain.BookmarkBase;
import com.freerdp.freerdpcore.domain.ConnectionReference;
import com.freerdp.freerdpcore.domain.ManualBookmark;
import com.freerdp.freerdpcore.domain.PlaceholderBookmark;
import com.freerdp.freerdpcore.presentation.BookmarkActivity;

public class BookmarkArrayAdapter extends ArrayAdapter<BookmarkBase>
{

	public BookmarkArrayAdapter(Context context, int textViewResourceId, List<BookmarkBase> objects) 
	{
		super(context, textViewResourceId, objects);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent)
	{
    	View curView = convertView;
        if (curView == null) 
        {
            LayoutInflater vi = (LayoutInflater)getContext().getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            curView = vi.inflate(R.layout.bookmark_list_item, null);
        }            
        
        BookmarkBase bookmark = getItem(position);
        TextView label = (TextView) curView.findViewById(R.id.bookmark_text1);
        TextView hostname = (TextView) curView.findViewById(R.id.bookmark_text2);
        ImageView star_icon = (ImageView) curView.findViewById(R.id.bookmark_icon2);
        assert label != null;
        assert hostname != null;

       	label.setText(bookmark.getLabel());
       	star_icon.setVisibility(View.VISIBLE);
        
        String refStr;
    	if(bookmark.getType() == BookmarkBase.TYPE_MANUAL)
    	{
    		hostname.setText(bookmark.<ManualBookmark>get().getHostname());
        	refStr = ConnectionReference.getManualBookmarkReference(bookmark.getId());
        	star_icon.setImageResource(R.drawable.icon_star_on);
    	}
    	else if(bookmark.getType() == BookmarkBase.TYPE_QUICKCONNECT)
    	{
    		// just set an empty hostname (with a blank) - the hostname is already displayed in the label
    		// and in case we just set it to "" the textview will shrunk
    		hostname.setText(" ");
        	refStr = ConnectionReference.getHostnameReference(bookmark.getLabel());
        	star_icon.setImageResource(R.drawable.icon_star_off);
    	}
        else if(bookmark.getType() == BookmarkBase.TYPE_PLACEHOLDER)
        {
    		hostname.setText(" ");
        	refStr = ConnectionReference.getPlaceholderReference(bookmark.<PlaceholderBookmark>get().getName());
        	star_icon.setVisibility(View.GONE);        	        	
        }
        else
        {
        	// unknown bookmark type...
        	refStr = "";
        	assert false;
        }

    	star_icon.setOnClickListener(new OnClickListener() {
			@Override
			public void onClick(View v) {
				// start bookmark editor
				Bundle bundle = new Bundle();				
				String refStr = v.getTag().toString();
				bundle.putString(BookmarkActivity.PARAM_CONNECTION_REFERENCE, refStr);

				Intent bookmarkIntent = new Intent(getContext(), BookmarkActivity.class);
				bookmarkIntent.putExtras(bundle);				
				getContext().startActivity(bookmarkIntent);					
			}
    	});
    	
        curView.setTag(refStr);
        star_icon.setTag(refStr);        
       
        return curView;
	}

	public void addItems(List<BookmarkBase> newItems)
	{
		for(BookmarkBase item : newItems)
			add(item);
	}
	
	public void replaceItems(List<BookmarkBase> newItems) 
	{
		clear();
		for(BookmarkBase item : newItems)
			add(item);
	}
	
	public void remove(long bookmarkId)
	{
		for(int i = 0; i < getCount(); i++)
		{
			BookmarkBase bm = getItem(i);
			if(bm.getId() == bookmarkId)
			{
				remove(bm);
				return;
			}
		}
	}
}
