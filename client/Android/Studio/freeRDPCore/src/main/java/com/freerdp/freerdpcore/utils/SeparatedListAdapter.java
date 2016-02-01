/*
   Separated List Adapter
   Taken from http://jsharkey.org/blog/2008/08/18/separating-lists-with-headers-in-android-09/

   Copyright Jeff Sharkey

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import java.util.LinkedHashMap;
import java.util.Map;

import com.freerdp.freerdpcore.R;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Adapter;
import android.widget.ArrayAdapter;
import android.widget.BaseAdapter;

public class SeparatedListAdapter extends BaseAdapter {  
    
    public final Map<String, Adapter> sections = new LinkedHashMap<String,Adapter>();  
    public final ArrayAdapter<String> headers;  
    public final static int TYPE_SECTION_HEADER = 0;  
  
    public SeparatedListAdapter(Context context) {  
        headers = new ArrayAdapter<String>(context, R.layout.list_header);  
    }  
  
    public void addSection(String section, Adapter adapter) {  
        this.headers.add(section);  
        this.sections.put(section, adapter);  
    }  

    public void setSectionTitle(int section, String title) {
    	String oldTitle = this.headers.getItem(section);

    	// remove/add to headers array
    	this.headers.remove(oldTitle);
    	this.headers.insert(title, section);
    	
    	// remove/add to section map
    	Adapter adapter = this.sections.get(oldTitle);
    	this.sections.remove(oldTitle);
    	this.sections.put(title, adapter);
    }
    
    public Object getItem(int position) {  
        for(int i = 0; i < headers.getCount(); i++) {
        	String section = headers.getItem(i);
            Adapter adapter = sections.get(section);
            
            // ignore empty sections
            if(adapter.getCount() > 0)
            {
                int size = adapter.getCount() + 1;  
                
                // check if position inside this section  
                if(position == 0) return section;  
                if(position < size) return adapter.getItem(position - 1);  
      
                // otherwise jump into next section  
                position -= size;              	
            }
        }  
        return null;  
    }  
  
    public int getCount() {  
        // total together all sections, plus one for each section header (except if the section is empty)  
        int total = 0;  
        for(Adapter adapter : this.sections.values())  
            total += ((adapter.getCount() > 0) ? adapter.getCount() + 1 : 0);  
        return total;  
    }  
  
    public int getViewTypeCount() {  
        // assume that headers count as one, then total all sections  
        int total = 1;  
        for(Adapter adapter : this.sections.values())  
            total += adapter.getViewTypeCount();  
        return total;  
    }  
  
    public int getItemViewType(int position) {  
        int type = 1;  
        for(int i = 0; i < headers.getCount(); i++) {
        	String section = headers.getItem(i);
            Adapter adapter = sections.get(section);  
            
            // skip empty sections
            if(adapter.getCount() > 0)
            {
                int size = adapter.getCount() + 1;  
                
                // check if position inside this section  
                if(position == 0) return TYPE_SECTION_HEADER;
                if(position < size) return type + adapter.getItemViewType(position - 1);  
      
                // otherwise jump into next section  
                position -= size;  
                type += adapter.getViewTypeCount();              	
            }
        }  
        return -1;  
    }  
  
    public boolean areAllItemsSelectable() {  
        return false;  
    }  
  
    public boolean isEnabled(int position) {  
        return (getItemViewType(position) != TYPE_SECTION_HEADER);  
    }  
  
    @Override  
    public View getView(int position, View convertView, ViewGroup parent) {  
        int sectionnum = 0;  
        for(int i = 0; i < headers.getCount(); i++) {
        	String section = headers.getItem(i);
            Adapter adapter = sections.get(section);  
            
            // skip empty sections
            if(adapter.getCount() > 0)
            {
                int size = adapter.getCount() + 1;  
                
                // check if position inside this section               
                if(position == 0) return headers.getView(sectionnum, convertView, parent);  
                if(position < size) return adapter.getView(position - 1, null, parent);  
      
                // otherwise jump into next section  
                position -= size;  
            }            
            sectionnum++;              	
        }  
        return null;  
    }  
  
    @Override  
    public long getItemId(int position) {  
        for(int i = 0; i < headers.getCount(); i++) 
        {
        	String section = headers.getItem(i);
            Adapter adapter = sections.get(section);
            if(adapter.getCount() > 0)
            {
                int size = adapter.getCount() + 1;  
                
                // check if position inside this section  
                if(position < size) return adapter.getItemId(position - 1);  
      
                // otherwise jump into next section  
                position -= size;              	
            }
        }  
        return -1;  
    }  

    public String getSectionForPosition(int position) {
    	int curPos = 0;
        for(int i = 0; i < headers.getCount(); i++) 
        {
        	String section = headers.getItem(i);
            Adapter adapter = sections.get(section);  
            if(adapter.getCount() > 0)
            {
                int size = adapter.getCount() + 1;
                
                // check if position inside this section  
                if(position >= curPos && position < (curPos + size)) return section.toString();  
      
                // otherwise jump into next section  
                curPos += size;            	
            }
        }  
        return null;  
    }    
  
}  