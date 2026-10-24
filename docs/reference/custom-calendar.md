# Fixed calendars

Project settings → Custom calendar lets a designer replace the twelve-month calendar with fixed-length phases, seasons, school terms or other units. Enable the calendar, choose 1–60 units per year and the days in each unit, then name the year, unit, activity slot, day and seven weekdays. The starting weekday belongs to day one of the project start date.

An eight-year game with thirteen four-week phases uses `monthsPerYear: 13`, `daysPerMonth: 28`, `durationMonths: 104` and `periodsPerMonth: 4`. Labels can be Year, Phase, Week and Day. Its last activity is Year 8 · Phase 13 · Week 4 when starting at Year 1 · Phase 1.

The existing data names `startMonth`, `durationMonths`, `periodsPerMonth`, `time.month` and `time.periodInMonth` remain stable: month means an authored unit. Event windows still store zero-based activity indices. The visual story-flow form retains its existing one-based authoring ordinal. Calendar settings do not rewrite event windows or migrate old saves.

Calendar days control presentation. To apply daily gameplay rules, enable the existing rule calendar and use the same day count. The last activity slot owns any remaining days; three-slot calendars retain the existing three-way logical-day partition. The schedule grid uses these same bounds and fixed weekday arithmetic.

Projects without `calendar` retain their twelve-month dates and Gregorian presentation. Any explicit calendar requires the `calendar-v1` player capability, including fixed calendars with twelve units. Old exported players are rejected by export compatibility checks. Calendar labels and weekday names use the normal localization catalog.

The feature does not add leap days, variable unit lengths or a new simulation clock. Existing time conditions, boundary rules, event queues and save journals remain authoritative.
