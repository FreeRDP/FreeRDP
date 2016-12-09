/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Time Zone Redirection Table Generator
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

using System;
using System.IO;
using System.Globalization;
using System.Collections.ObjectModel;

namespace TimeZones
{
    struct SYSTEM_TIME_ENTRY
    {
        public UInt16 wYear;
        public UInt16 wMonth;
        public UInt16 wDayOfWeek;
        public UInt16 wDay;
        public UInt16 wHour;
        public UInt16 wMinute;
        public UInt16 wSecond;
        public UInt16 wMilliseconds;
    };

    struct TIME_ZONE_RULE_ENTRY
    {
        public long TicksStart;
        public long TicksEnd;
        public Int32 DaylightDelta;
        public SYSTEM_TIME_ENTRY StandardDate;
        public SYSTEM_TIME_ENTRY DaylightDate;
    };

    struct TIME_ZONE_ENTRY
    {
        public string Id;
        public Int32 Bias;
        public bool SupportsDST;
        public string DisplayName;
        public string StandardName;
        public string DaylightName;
        public string RuleTable;
        public UInt32 RuleTableCount;
    };

    class TimeZones
    {
        static void Main(string[] args)
        {
            int i;
            UInt32 index;
            const string file = @"TimeZones.txt";
            TimeZoneInfo.AdjustmentRule[] rules;
            StreamWriter stream = new StreamWriter(file, false);
            ReadOnlyCollection<TimeZoneInfo> timeZones = TimeZoneInfo.GetSystemTimeZones();

            stream.WriteLine();

            stream.WriteLine("struct _TIME_ZONE_RULE_ENTRY");
            stream.WriteLine("{");
            stream.WriteLine("\tUINT64 TicksStart;");
            stream.WriteLine("\tUINT64 TicksEnd;");
            stream.WriteLine("\tINT32 DaylightDelta;");
            stream.WriteLine("\tSYSTEMTIME StandardDate;");
            stream.WriteLine("\tSYSTEMTIME DaylightDate;");
            stream.WriteLine("};");
            stream.WriteLine("typedef struct _TIME_ZONE_RULE_ENTRY TIME_ZONE_RULE_ENTRY;");
            stream.WriteLine();

            stream.WriteLine("struct _TIME_ZONE_ENTRY");
            stream.WriteLine("{");
            stream.WriteLine("\tconst char* Id;");
            stream.WriteLine("\tINT32 Bias;");
            stream.WriteLine("\tBOOL SupportsDST;");
            stream.WriteLine("\tconst char* DisplayName;");
            stream.WriteLine("\tconst char* StandardName;");
            stream.WriteLine("\tconst char* DaylightName;");
            stream.WriteLine("\tTIME_ZONE_RULE_ENTRY* RuleTable;");
            stream.WriteLine("\tUINT32 RuleTableCount;");
            stream.WriteLine("};");
            stream.WriteLine("typedef struct _TIME_ZONE_ENTRY TIME_ZONE_ENTRY;");
            stream.WriteLine();

            index = 0;

            foreach (TimeZoneInfo timeZone in timeZones)
            {
                rules = timeZone.GetAdjustmentRules();

                if ((!timeZone.SupportsDaylightSavingTime) || (rules.Length < 1))
                {
                    index++;
                    continue;
                }

                stream.WriteLine("static const TIME_ZONE_RULE_ENTRY TimeZoneRuleTable_{0}[] =", index);
                stream.WriteLine("{");

                i = 0;
                foreach (TimeZoneInfo.AdjustmentRule rule in rules)
                {
                    DateTime time;
                    TIME_ZONE_RULE_ENTRY tzr;
                    TimeZoneInfo.TransitionTime transition;

                    tzr.TicksStart = rule.DateEnd.ToUniversalTime().Ticks;
                    tzr.TicksEnd = rule.DateStart.ToUniversalTime().Ticks;
                    tzr.DaylightDelta = (Int32)rule.DaylightDelta.TotalMinutes;

                    transition = rule.DaylightTransitionEnd;
                    time = transition.TimeOfDay;

                    tzr.StandardDate.wYear = (UInt16)0;
                    tzr.StandardDate.wMonth = (UInt16)transition.Month;
                    tzr.StandardDate.wDayOfWeek = (UInt16)transition.DayOfWeek;
                    tzr.StandardDate.wDay = (UInt16)transition.Week;
                    tzr.StandardDate.wHour = (UInt16)time.Hour;
                    tzr.StandardDate.wMinute = (UInt16)time.Minute;
                    tzr.StandardDate.wSecond = (UInt16)time.Second;
                    tzr.StandardDate.wMilliseconds = (UInt16)time.Millisecond;

                    transition = rule.DaylightTransitionStart;
                    time = transition.TimeOfDay;

                    tzr.DaylightDate.wYear = (UInt16)0;
                    tzr.DaylightDate.wMonth = (UInt16)transition.Month;
                    tzr.DaylightDate.wDayOfWeek = (UInt16)transition.DayOfWeek;
                    tzr.DaylightDate.wDay = (UInt16)transition.Week;
                    tzr.DaylightDate.wHour = (UInt16)time.Hour;
                    tzr.DaylightDate.wMinute = (UInt16)time.Minute;
                    tzr.DaylightDate.wSecond = (UInt16)time.Second;
                    tzr.DaylightDate.wMilliseconds = (UInt16)time.Millisecond;

                    stream.Write("\t{");
                    stream.Write(" {0}ULL, {1}ULL, {2},", tzr.TicksStart, tzr.TicksEnd, tzr.DaylightDelta);

                    stream.Write(" { ");
                    stream.Write("{0}, {1}, {2}, {3}, {4}, {5}",
                        tzr.StandardDate.wYear, tzr.StandardDate.wMonth, tzr.StandardDate.wDayOfWeek,
                        tzr.StandardDate.wDay, tzr.StandardDate.wHour, tzr.StandardDate.wMinute,
                        tzr.StandardDate.wSecond, tzr.StandardDate.wMilliseconds);
                    stream.Write(" }, ");

                    stream.Write("{ ");
                    stream.Write("{0}, {1}, {2}, {3}, {4}, {5}",
                        tzr.DaylightDate.wYear, tzr.DaylightDate.wMonth, tzr.DaylightDate.wDayOfWeek,
                        tzr.DaylightDate.wDay, tzr.DaylightDate.wHour, tzr.DaylightDate.wMinute,
                        tzr.DaylightDate.wSecond, tzr.DaylightDate.wMilliseconds);
                    stream.Write(" },");

                    if (++i < rules.Length)
                        stream.WriteLine(" },");
                    else
                        stream.WriteLine(" }");
                }

                stream.WriteLine("};");
                stream.WriteLine();
                index++;
            }

            index = 0;
            stream.WriteLine("static const TIME_ZONE_ENTRY TimeZoneTable[] =");
            stream.WriteLine("{");

            foreach (TimeZoneInfo timeZone in timeZones)
            {
                TIME_ZONE_ENTRY tz;
                TimeSpan offset = timeZone.BaseUtcOffset;

                rules = timeZone.GetAdjustmentRules();

                tz.Id = timeZone.Id;
		tz.Bias = -(Int32)offset.TotalMinutes;

                tz.SupportsDST = timeZone.SupportsDaylightSavingTime;

                tz.DisplayName = timeZone.DisplayName;
                tz.StandardName = timeZone.StandardName;
                tz.DaylightName = timeZone.DaylightName;

                if ((!tz.SupportsDST) || (rules.Length < 1))
                {
                    tz.RuleTableCount = 0;
                    tz.RuleTable = "NULL";
                }
                else
                {
                    tz.RuleTableCount = (UInt32)rules.Length;
                    tz.RuleTable = "&TimeZoneRuleTable_" + index;
                    tz.RuleTable = "(TIME_ZONE_RULE_ENTRY*) &TimeZoneRuleTable_" + index;
                }

                stream.WriteLine("\t{");

                stream.WriteLine("\t\t\"{0}\", {1}, {2}, \"{3}\",",
                    tz.Id, tz.Bias, tz.SupportsDST ? "TRUE" : "FALSE", tz.DisplayName);

                stream.WriteLine("\t\t\"{0}\", \"{1}\",", tz.StandardName, tz.DaylightName);
                stream.WriteLine("\t\t{0}, {1}", tz.RuleTable, tz.RuleTableCount);

                index++;

                if ((int)index < timeZones.Count)
                    stream.WriteLine("\t},");
                else
                    stream.WriteLine("\t}");
            }
            stream.WriteLine("};");
            stream.WriteLine();

            stream.Close();
        }
    }
}

