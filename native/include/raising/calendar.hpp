#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
namespace raising::dates {
struct Spec { bool custom=false; int months=12,days=30,startWeekday=0; std::string year="Year",month="Month",period="Period",day="Day";std::array<std::string,7> weekdays{{"Sun","Mon","Tue","Wed","Thu","Fri","Sat"}}; };
struct Date { int year,month,period;int64_t index; };
inline Date date(int startYear,int startMonth,int count,int64_t index,int months=12){const auto absolute=startMonth-1+index/count;return {int(startYear+absolute/months),int(absolute%months+1),int(index%count),index};}
inline std::string text(const Spec& c,const Date& d,bool period=true){return c.year+" "+std::to_string(d.year)+" · "+c.month+" "+std::to_string(d.month)+(period?" · "+c.period+" "+std::to_string(d.period+1):"");}
inline int slot_start(int slot,int count,int days){return (count==3?slot*days/3:slot*(days/count))+1;}
struct Grid {int days,offset,rows,day;};
inline Grid grid(const Spec& c,const Date& d,int startYear,int startMonth,int count){
 if(c.custom){int elapsed=(d.year-startYear)*c.months+d.month-startMonth,offset=(c.startWeekday+elapsed*c.days)%7;return {c.days,offset,(offset+c.days+6)/7,slot_start(d.period,count,c.days)};}
 const int lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};int days=lengths[std::clamp(d.month-1,0,11)]+(d.month==2&&d.year%4==0&&(d.year%100!=0||d.year%400==0)?1:0);
 int y=d.year,m=d.month;if(m<3){m+=12;--y;}int offset=(1+13*(m+1)/5+y+y/4-y/100+y/400+6)%7;
 return {days,offset,6,d.period*(count==3?10:days/count)+1};
}
inline int day_slot(int day,int count,int days,bool custom){if(!custom)return std::min(count-1,(day-1)/(count==3?10:days/count));int slot=0;while(slot+1<count&&day>=slot_start(slot+1,count,days))++slot;return slot;}
} // namespace raising::dates
