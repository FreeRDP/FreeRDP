/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * TZID to Windows TimeZone Identifier Table Generator
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#r "System.Xml"

using System;
using System.IO;
using System.Xml;
using System.Text;
using System.Collections;
using System.Collections.Generic;

/* 
 * this script uses windowsZones.xml which can be obtained at:
 * http://www.unicode.org/repos/cldr/tags/latest/common/supplemental/windowsZones.xml
 */

string tzid, windows;
const string file = @"WindowsZones.txt";
const string zonesUrl = @"http://www.unicode.org/repos/cldr/tags/latest/common/supplemental/windowsZones.xml";
List<string> list = new List<string>();
StreamWriter stream = new StreamWriter(file, false);
XmlTextReader reader = new XmlTextReader(zonesUrl);

stream.WriteLine("struct _WINDOWS_TZID_ENTRY");
stream.WriteLine("{");
stream.WriteLine("\tconst char* windows;");
stream.WriteLine("\tconst char* tzid;");
stream.WriteLine("};");
stream.WriteLine("typedef struct _WINDOWS_TZID_ENTRY WINDOWS_TZID_ENTRY;");
stream.WriteLine();

while (reader.Read())
{
	switch (reader.NodeType)
	{
		case XmlNodeType.Element:

			if (reader.Name.Equals("mapZone"))
			{
				tzid = reader.GetAttribute("type");
				windows = reader.GetAttribute("other");
			
				string entry = String.Format("\"{0}\", \"{1}\"", windows, tzid);
				
				if (!list.Contains(entry))
					list.Add(entry);
			}
		
			break;
	}
}

list.Sort();

stream.WriteLine("const WINDOWS_TZID_ENTRY WindowsTimeZoneIdTable[] =");
stream.WriteLine("{");

foreach (string entry in list)
{
		stream.Write("\t{ ");
		stream.Write(entry);
		stream.WriteLine(" },");	
}

stream.WriteLine("};");

stream.Close();
