/*
   Simple .RDP file parser

   Copyright 2013 Blaz Bacnik

   This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. 
   If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

package com.freerdp.freerdpcore.utils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Locale;

public class RDPFileParser {
	
	private static final int MAX_ERRORS = 20;
	private static final int MAX_LINES = 500;
	
	private HashMap<String, Object> options;

	private void init()
	{
		options = new HashMap<String, Object>(); 
	}
	
	public RDPFileParser()
	{
		init();
	}
	
	public RDPFileParser(String filename) throws IOException
	{
		init();
		parse(filename);
	}
	
	public void parse(String filename) throws IOException
	{
		BufferedReader br = new BufferedReader(new FileReader(filename));
		String line = null;
		
		int errors = 0;
		int lines = 0;
		boolean ok;
	
		while ((line = br.readLine()) != null)
		{
			lines++; ok = false;
			
			if (errors > MAX_ERRORS || lines > MAX_LINES)
			{
				br.close();			
				throw new IOException("Parsing limits exceeded");
			}
			
			String[] fields = line.split(":", 3);
			
			if (fields.length == 3)
			{
				if (fields[1].equals("s"))
				{
					options.put(fields[0].toLowerCase(Locale.ENGLISH), fields[2]);
					ok = true;
				}
				else if (fields[1].equals("i"))
				{
					try
					{
						Integer i = Integer.parseInt(fields[2]);
						options.put(fields[0].toLowerCase(Locale.ENGLISH), i);
						ok = true;
					}
					catch (NumberFormatException e) { }
				}
				else if (fields[1].equals("b"))
				{
					ok = true;
				}
			}
			
			if (!ok) errors++;
		}
		br.close();
	}
	
	public String getString(String optionName)
	{
		if (options.get(optionName) instanceof String)
			return (String) options.get(optionName);
		else
			return null;
	}

	public Integer getInteger(String optionName)
	{
		if (options.get(optionName) instanceof Integer)
			return (Integer) options.get(optionName);
		else
			return null;
	}
}
