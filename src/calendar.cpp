#include "autopoiesis/calendar.hpp"

#include <algorithm>

namespace apo {
CalendarDate date_from_absolute_day(int absolute_day) {
  absolute_day=std::max(1,absolute_day);
  const int zero_based=absolute_day-1;
  const int day_of_year=zero_based%360;
  const int month=day_of_year/30+1;
  Season season=Season::Spring;
  if(month>=10)season=Season::Winter;
  else if(month>=7)season=Season::Autumn;
  else if(month>=4)season=Season::Summer;
  return {absolute_day,day_of_year%30+1,month,zero_based/360+1,season};
}

ClimateState climate_for(const CalendarDate& date) {
  int baseline=15;
  int rainfall=4;
  if(date.season==Season::Summer){baseline=27;rainfall=date.day_of_month%7==0?8:1;}
  else if(date.season==Season::Autumn){baseline=14;rainfall=date.day_of_month%3==0?10:3;}
  else if(date.season==Season::Winter){baseline=3;rainfall=date.day_of_month%5==0?8:2;}
  else rainfall=date.day_of_month%4==0?12:4;
  const int temperature=baseline+(date.absolute_day*37)%7-3;
  std::string condition="clair";
  if(temperature<=2)condition="gel";
  else if(temperature>=29)condition="forte chaleur";
  else if(rainfall>=8)condition="pluie";
  return {temperature,rainfall,condition};
}

std::string season_name(Season season) {
  switch(season){
    case Season::Spring:return "printemps";
    case Season::Summer:return "été";
    case Season::Autumn:return "automne";
    case Season::Winter:return "hiver";
  }
  return "inconnue";
}

std::string calendar_label(const CalendarDate& date) {
  return "jour "+std::to_string(date.day_of_month)+", mois "+std::to_string(date.month)+
         ", année "+std::to_string(date.year)+" ("+season_name(date.season)+")";
}

json calendar_json(const CalendarDate& date) {
  return {{"absolute_day",date.absolute_day},{"day_of_month",date.day_of_month},
          {"month",date.month},{"year",date.year},{"season",season_name(date.season)},
          {"label",calendar_label(date)}};
}

json climate_json(const ClimateState& climate) {
  return {{"temperature_c",climate.temperature_c},{"rainfall_mm",climate.rainfall_mm},
          {"condition",climate.condition}};
}
}
