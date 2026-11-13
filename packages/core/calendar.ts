import type { ProjectDefinition } from '../schema/generated/project.js';
export type Calendar = Readonly<Omit<NonNullable<ProjectDefinition['calendar']>,'weekdayLabels'>> & {readonly weekdayLabels?:readonly string[]};
export interface CalendarProject {startYear:number;startMonth:number;periodsPerMonth:number;calendar?:Readonly<Calendar>}
export interface CalendarDate {year:number;month:number;periodInMonth:number;periodIndex:number}
/** Stored month/period fields remain stable; a custom calendar gives them authored units. */
export function calendarDate(project:CalendarProject,periodIndex:number):CalendarDate {
 const months=project.calendar?.monthsPerYear??12,absolute=project.startMonth-1+Math.floor(periodIndex/project.periodsPerMonth);
 return {periodIndex,year:project.startYear+Math.floor(absolute/months),month:absolute%months+1,periodInMonth:periodIndex%project.periodsPerMonth};
}
export function calendarProblem(project:CalendarProject & {gameplay?:{ruleCalendar?:{daysPerMonth:number}}}):{path:string;message:string}|null {
 const c=project.calendar;
 if(c!==undefined&&(!c||typeof c!=='object'||Array.isArray(c)))return {path:'/calendar',message:'Calendar must be an object.'};
 if(c){
  if(!Number.isInteger(c.monthsPerYear)||c.monthsPerYear<1||c.monthsPerYear>60)return {path:'/calendar/monthsPerYear',message:'Calendar units per year must be an integer from 1 to 60.'};
  if(!Number.isInteger(c.daysPerMonth)||c.daysPerMonth<project.periodsPerMonth||c.daysPerMonth>366)return {path:'/calendar/daysPerMonth',message:'Calendar days per unit must be at least the number of slots and at most 366.'};
  if(c.startWeekday!==undefined&&(!Number.isInteger(c.startWeekday)||c.startWeekday<0||c.startWeekday>6))return {path:'/calendar/startWeekday',message:'Starting weekday must be from 0 to 6.'};
  for(const key of ['yearLabel','monthLabel','periodLabel','dayLabel'] as const)if(c[key]!==undefined&&(typeof c[key]!=='string'||!c[key]!.trim()||[...c[key]!].length>80))return {path:'/calendar/'+key,message:'Calendar labels must contain 1–80 characters.'};
  if(c.weekdayLabels!==undefined&&(!Array.isArray(c.weekdayLabels)||c.weekdayLabels.length!==7||c.weekdayLabels.some(x=>typeof x!=='string'||!x.trim()||[...x].length>80)))return {path:'/calendar/weekdayLabels',message:'Provide exactly seven nonblank weekday labels.'};
  if(project.gameplay?.ruleCalendar&&project.gameplay.ruleCalendar.daysPerMonth!==c.daysPerMonth)return {path:'/gameplay/ruleCalendar/daysPerMonth',message:'Logical rule days must match the custom calendar days per unit.'};
 }
 if(!Number.isInteger(project.startMonth)||project.startMonth<1||project.startMonth>(c?.monthsPerYear??12))return {path:'/startMonth',message:'Starting unit is outside the calendar year.'};
 return null;
}
export function calendarLabels(c:Readonly<Calendar>,locale='en') {
 const ko=locale.startsWith('ko'),ja=locale.startsWith('ja');
 return {year:c.yearLabel??(ko?'연도':ja?'年':'Year'),month:c.monthLabel??(ko?'월':ja?'月':'Month'),period:c.periodLabel??(ko?'일정':ja?'期間':'Period'),day:c.dayLabel??(ko?'일':ja?'日':'Day'),weekdays:c.weekdayLabels??(ko?['일','월','화','수','목','금','토']:ja?['日','月','火','水','木','金','土']:['Sun','Mon','Tue','Wed','Thu','Fri','Sat'])};
}
export function calendarText(date:CalendarDate,c:Readonly<Calendar>,locale='en',withPeriod=true):string {
 const l=calendarLabels(c,locale);return `${l.year} ${date.year} · ${l.month} ${date.month}`+(withPeriod?` · ${l.period} ${date.periodInMonth+1}`:'');
}
/** Matches logical-day gameplay/timeline partitions; the final slot owns the remainder. */
export function calendarSlotStart(slot:number,count:number,days:number):number {return (count===3?Math.floor(slot*days/3):slot*Math.floor(days/count))+1;}
export function calendarGrid(project:CalendarProject,date:CalendarDate) {
 const c=project.calendar;
 if(c){const elapsed=(date.year-project.startYear)*c.monthsPerYear+date.month-project.startMonth;const offset=((c.startWeekday??0)+elapsed*c.daysPerMonth)%7;return {days:c.daysPerMonth,offset,rows:Math.ceil((offset+c.daysPerMonth)/7),day:calendarSlotStart(date.periodInMonth,project.periodsPerMonth,c.daysPerMonth)};}
 const d=new Date(0);d.setUTCHours(0,0,0,0);d.setUTCFullYear(date.year,date.month-1,1);const offset=d.getUTCDay();d.setUTCMonth(date.month,0);const days=d.getUTCDate();return {days,offset,rows:6,day:date.periodInMonth*(project.periodsPerMonth===3?10:Math.floor(days/project.periodsPerMonth))+1};
}
export function calendarDaySlot(day:number,count:number,days:number,custom:boolean):number {
 if(!custom)return Math.min(count-1,Math.floor((day-1)/(count===3?10:Math.floor(days/count))));
 let slot=0;while(slot+1<count&&day>=calendarSlotStart(slot+1,count,days))slot++;return slot;
}
