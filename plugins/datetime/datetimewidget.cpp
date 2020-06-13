/*
 * Copyright (C) 2011 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "datetimewidget.h"
#include "constants.h"

#include <QApplication>
#include <QPainter>
#include <QDebug>
#include <QSvgRenderer>
#include <QMouseEvent>
#include <DFontSizeManager>
#include <DGuiApplicationHelper>

#define PLUGIN_STATE_KEY    "enable"
#define TIME_FONT DFontSizeManager::instance()->t3()


class Lunar
{
public:
    const  QStringList 天干 = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
    const  QStringList 地支 = {"子", ",丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
    const  QStringList 生肖 = {"鼠","牛","虎","兔","龙","蛇","马","羊","猴","鸡","狗","猪"};
    const  QStringList 节气 = {"小寒","大寒","立春","雨水","惊蛰","春分","清明","谷雨","立夏","小满","芒种","夏至","小暑","大暑","立秋","处暑","白露","秋分","寒露","霜降","立冬","小雪","大雪","冬至"};
    const  QStringList 日 = {"日","一","二","三","四","五","六","七","八","九","十"};
    const  QStringList 十 = {"初","十","廿","卅"};
    const  QStringList 月 = {"正","一","二","三","四","五","六","七","八","九","十","冬","腊"};
    /**
      * 公历每个月份的天数普通表
      * @Array Of Property
      * @return Number 
      */
    const  int solarMonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

private:

    unsigned long int lunarInfo[201]= {0x04bd8,0x04ae0,0x0a570,0x054d5,0x0d260,0x0d950,0x16554,0x056a0,0x09ad0,0x055d2,//1900-1909
            0x04ae0,0x0a5b6,0x0a4d0,0x0d250,0x1d255,0x0b540,0x0d6a0,0x0ada2,0x095b0,0x14977,//1910-1919
            0x04970,0x0a4b0,0x0b4b5,0x06a50,0x06d40,0x1ab54,0x02b60,0x09570,0x052f2,0x04970,//1920-1929
            0x06566,0x0d4a0,0x0ea50,0x06e95,0x05ad0,0x02b60,0x186e3,0x092e0,0x1c8d7,0x0c950,//1930-1939
            0x0d4a0,0x1d8a6,0x0b550,0x056a0,0x1a5b4,0x025d0,0x092d0,0x0d2b2,0x0a950,0x0b557,//1940-1949
            0x06ca0,0x0b550,0x15355,0x04da0,0x0a5b0,0x14573,0x052b0,0x0a9a8,0x0e950,0x06aa0,//1950-1959
            0x0aea6,0x0ab50,0x04b60,0x0aae4,0x0a570,0x05260,0x0f263,0x0d950,0x05b57,0x056a0,//1960-1969
            0x096d0,0x04dd5,0x04ad0,0x0a4d0,0x0d4d4,0x0d250,0x0d558,0x0b540,0x0b6a0,0x195a6,//1970-1979
            0x095b0,0x049b0,0x0a974,0x0a4b0,0x0b27a,0x06a50,0x06d40,0x0af46,0x0ab60,0x09570,//1980-1989
            0x04af5,0x04970,0x064b0,0x074a3,0x0ea50,0x06b58,0x055c0,0x0ab60,0x096d5,0x092e0,//1990-1999
            0x0c960,0x0d954,0x0d4a0,0x0da50,0x07552,0x056a0,0x0abb7,0x025d0,0x092d0,0x0cab5,//2000-2009
            0x0a950,0x0b4a0,0x0baa4,0x0ad50,0x055d9,0x04ba0,0x0a5b0,0x15176,0x052b0,0x0a930,//2010-2019
            0x07954,0x06aa0,0x0ad50,0x05b52,0x04b60,0x0a6e6,0x0a4e0,0x0d260,0x0ea65,0x0d530,//2020-2029
            0x05aa0,0x076a3,0x096d0,0x04afb,0x04ad0,0x0a4d0,0x1d0b6,0x0d250,0x0d520,0x0dd45,//2030-2039
            0x0b5a0,0x056d0,0x055b2,0x049b0,0x0a577,0x0a4b0,0x0aa50,0x1b255,0x06d20,0x0ada0,//2040-2049
            0x14b63,0x09370,0x049f8,0x04970,0x064b0,0x168a6,0x0ea50, 0x06b20,0x1a6c4,0x0aae0,//2050-2059
            0x0a2e0,0x0d2e3,0x0c960,0x0d557,0x0d4a0,0x0da50,0x05d55,0x056a0,0x0a6d0,0x055d4,//2060-2069
            0x052d0,0x0a9b8,0x0a950,0x0b4a0,0x0b6a6,0x0ad50,0x055a0,0x0aba4,0x0a5b0,0x052b0,//2070-2079
            0x0b273,0x06930,0x07337,0x06aa0,0x0ad50,0x14b55,0x04b60,0x0a570,0x054e4,0x0d160,//2080-2089
            0x0e968,0x0d520,0x0daa0,0x16aa6,0x056d0,0x04ae0,0x0a9d4,0x0a2d0,0x0d150,0x0f252,//2090-2099
            0x0d520 //2100
    };

    /**
      * 1900-2100各年的24节气日期速查表
      * @Array Of Property 
      * @return 0x string For splice
      */
    QStringList sTermInfo = {"9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e","97bcf97c3598082c95f8c965cc920f",
              "97bd0b06bdb0722c965ce1cfcc920f","b027097bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e",
              "97bcf97c359801ec95f8c965cc920f","97bd0b06bdb0722c965ce1cfcc920f","b027097bd097c36b0b6fc9274c91aa",
              "97b6b97bd19801ec9210c965cc920e","97bcf97c359801ec95f8c965cc920f","97bd0b06bdb0722c965ce1cfcc920f",
              "b027097bd097c36b0b6fc9274c91aa","9778397bd19801ec9210c965cc920e","97b6b97bd19801ec95f8c965cc920f",
              "97bd09801d98082c95f8e1cfcc920f","97bd097bd097c36b0b6fc9210c8dc2","9778397bd197c36c9210c9274c91aa",
              "97b6b97bd19801ec95f8c965cc920e","97bd09801d98082c95f8e1cfcc920f","97bd097bd097c36b0b6fc9210c8dc2",
              "9778397bd097c36c9210c9274c91aa","97b6b97bd19801ec95f8c965cc920e","97bcf97c3598082c95f8e1cfcc920f",
              "97bd097bd097c36b0b6fc9210c8dc2","9778397bd097c36c9210c9274c91aa","97b6b97bd19801ec9210c965cc920e",
              "97bcf97c3598082c95f8c965cc920f","97bd097bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b97bd19801ec9210c965cc920e","97bcf97c3598082c95f8c965cc920f","97bd097bd097c35b0b6fc920fb0722",
              "9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e","97bcf97c359801ec95f8c965cc920f",
              "97bd097bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e",
              "97bcf97c359801ec95f8c965cc920f","97bd097bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b97bd19801ec9210c965cc920e","97bcf97c359801ec95f8c965cc920f","97bd097bd07f595b0b6fc920fb0722",
              "9778397bd097c36b0b6fc9210c8dc2","9778397bd19801ec9210c9274c920e","97b6b97bd19801ec95f8c965cc920f",
              "97bd07f5307f595b0b0bc920fb0722","7f0e397bd097c36b0b6fc9210c8dc2","9778397bd097c36c9210c9274c920e",
              "97b6b97bd19801ec95f8c965cc920f","97bd07f5307f595b0b0bc920fb0722","7f0e397bd097c36b0b6fc9210c8dc2",
              "9778397bd097c36c9210c9274c91aa","97b6b97bd19801ec9210c965cc920e","97bd07f1487f595b0b0bc920fb0722",
              "7f0e397bd097c36b0b6fc9210c8dc2","9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e",
              "97bcf7f1487f595b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b97bd19801ec9210c965cc920e","97bcf7f1487f595b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722",
              "9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e","97bcf7f1487f531b0b0bb0b6fb0722",
              "7f0e397bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa","97b6b97bd19801ec9210c965cc920e",
              "97bcf7f1487f531b0b0bb0b6fb0722","7f0e397bd07f595b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b97bd19801ec9210c9274c920e","97bcf7f0e47f531b0b0bb0b6fb0722","7f0e397bd07f595b0b0bc920fb0722",
              "9778397bd097c36b0b6fc9210c91aa","97b6b97bd197c36c9210c9274c920e","97bcf7f0e47f531b0b0bb0b6fb0722",
              "7f0e397bd07f595b0b0bc920fb0722","9778397bd097c36b0b6fc9210c8dc2","9778397bd097c36c9210c9274c920e",
              "97b6b7f0e47f531b0723b0b6fb0722","7f0e37f5307f595b0b0bc920fb0722","7f0e397bd097c36b0b6fc9210c8dc2",
              "9778397bd097c36b0b70c9274c91aa","97b6b7f0e47f531b0723b0b6fb0721","7f0e37f1487f595b0b0bb0b6fb0722",
              "7f0e397bd097c35b0b6fc9210c8dc2","9778397bd097c36b0b6fc9274c91aa","97b6b7f0e47f531b0723b0b6fb0721",
              "7f0e27f1487f595b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722",
              "9778397bd097c36b0b6fc9274c91aa","97b6b7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722",
              "7f0e397bd097c35b0b6fc920fb0722","9778397bd097c36b0b6fc9274c91aa","97b6b7f0e47f531b0723b0b6fb0721",
              "7f0e27f1487f531b0b0bb0b6fb0722","7f0e397bd07f595b0b0bc920fb0722","9778397bd097c36b0b6fc9274c91aa",
              "97b6b7f0e47f531b0723b0787b0721","7f0e27f0e47f531b0b0bb0b6fb0722","7f0e397bd07f595b0b0bc920fb0722",
              "9778397bd097c36b0b6fc9210c91aa","97b6b7f0e47f149b0723b0787b0721","7f0e27f0e47f531b0723b0b6fb0722",
              "7f0e397bd07f595b0b0bc920fb0722","9778397bd097c36b0b6fc9210c8dc2","977837f0e37f149b0723b0787b0721",
              "7f07e7f0e47f531b0723b0b6fb0722","7f0e37f5307f595b0b0bc920fb0722","7f0e397bd097c35b0b6fc9210c8dc2",
              "977837f0e37f14998082b0787b0721","7f07e7f0e47f531b0723b0b6fb0721","7f0e37f1487f595b0b0bb0b6fb0722",
              "7f0e397bd097c35b0b6fc9210c8dc2","977837f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721",
              "7f0e27f1487f531b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722","977837f0e37f14998082b0787b06bd",
              "7f07e7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722","7f0e397bd097c35b0b6fc920fb0722",
              "977837f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722",
              "7f0e397bd07f595b0b0bc920fb0722","977837f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721",
              "7f0e27f1487f531b0b0bb0b6fb0722","7f0e397bd07f595b0b0bc920fb0722","977837f0e37f14998082b0787b06bd",
              "7f07e7f0e47f149b0723b0787b0721","7f0e27f0e47f531b0b0bb0b6fb0722","7f0e397bd07f595b0b0bc920fb0722",
              "977837f0e37f14998082b0723b06bd","7f07e7f0e37f149b0723b0787b0721","7f0e27f0e47f531b0723b0b6fb0722",
              "7f0e397bd07f595b0b0bc920fb0722","977837f0e37f14898082b0723b02d5","7ec967f0e37f14998082b0787b0721",
              "7f07e7f0e47f531b0723b0b6fb0722","7f0e37f1487f595b0b0bb0b6fb0722","7f0e37f0e37f14898082b0723b02d5",
              "7ec967f0e37f14998082b0787b0721","7f07e7f0e47f531b0723b0b6fb0722","7f0e37f1487f531b0b0bb0b6fb0722",
              "7f0e37f0e37f14898082b0723b02d5","7ec967f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721",
              "7f0e37f1487f531b0b0bb0b6fb0722","7f0e37f0e37f14898082b072297c35","7ec967f0e37f14998082b0787b06bd",
              "7f07e7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722","7f0e37f0e37f14898082b072297c35",
              "7ec967f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722",
              "7f0e37f0e366aa89801eb072297c35","7ec967f0e37f14998082b0787b06bd","7f07e7f0e47f149b0723b0787b0721",
              "7f0e27f1487f531b0b0bb0b6fb0722","7f0e37f0e366aa89801eb072297c35","7ec967f0e37f14998082b0723b06bd",
              "7f07e7f0e47f149b0723b0787b0721","7f0e27f0e47f531b0723b0b6fb0722","7f0e37f0e366aa89801eb072297c35",
              "7ec967f0e37f14998082b0723b06bd","7f07e7f0e37f14998083b0787b0721","7f0e27f0e47f531b0723b0b6fb0722",
              "7f0e37f0e366aa89801eb072297c35","7ec967f0e37f14898082b0723b02d5","7f07e7f0e37f14998082b0787b0721",
              "7f07e7f0e47f531b0723b0b6fb0722","7f0e36665b66aa89801e9808297c35","665f67f0e37f14898082b0723b02d5",
              "7ec967f0e37f14998082b0787b0721","7f07e7f0e47f531b0723b0b6fb0722","7f0e36665b66a449801e9808297c35",
              "665f67f0e37f14898082b0723b02d5","7ec967f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721",
              "7f0e36665b66a449801e9808297c35","665f67f0e37f14898082b072297c35","7ec967f0e37f14998082b0787b06bd",
              "7f07e7f0e47f531b0723b0b6fb0721","7f0e26665b66a449801e9808297c35","665f67f0e37f1489801eb072297c35",
              "7ec967f0e37f14998082b0787b06bd","7f07e7f0e47f531b0723b0b6fb0721","7f0e27f1487f531b0b0bb0b6fb0722"};

    /**
      * 返回农历y年一整年的总天数
      * @param lunar Year
      * @return Number
      */
    int lYearDays(int year)
    {
        int i, sum = 348;
        for(i=0x8000; i>0x8; i>>=1)
        { 
            sum += (lunarInfo[year-1900] & i) ? 1: 0;
        }
        return sum + leapDays(year);
    }

    /**
      * 返回农历y年闰月是哪个月；若y年没有闰月 则返回0
      * @param lunar Year
      * @return Number (0-12)
      */
    int getLeapMonth(int year) 
    {
        return lunarInfo[year-1900] & 0xf;
    }

    /**
      * 返回农历y年闰月的天数 若该年没有闰月则返回0
      * @param lunar Year
      * @return Number (0、29、30)
      * @eg:var leapMonthDay = calendar.leapDays(1987) ;//leapMonthDay=29
      */
    int leapDays(int year) 
    {
        if(getLeapMonth(year) > 0)  
        {
            return (lunarInfo[year-1900] & 0x10000) ? 30 : 29; 
        }
        return 0;
    }

    /**
      * 返回农历y年m月（非闰月）的总天数，计算m为闰月时的天数请使用leapDays方法
      * @param lunar Year
      * @return Number (-1、29、30)
      * @eg:var MonthDay = calendar.monthDays(1987,9) ;//MonthDay=29
      */
    int monthDays(int y,int m) 
    {
        if(m>12 || m<1) {return -1;}//月份参数从1至12，参数错误返回-1
        return( (lunarInfo[y-1900] & (0x10000>>m))? 30: 29 );
    }

        /**
      * 返回公历(!)y年m月的天数
      * @param solar Year
      * @return Number (-1、28、29、30、31)
      * @eg:var solarMonthDay = calendar.leapDays(1987) ;//solarMonthDay=30
      */
    int solarDays(int y, int m) {
        if(m>12 || m<1) {
            return -1;
        } //若参数错误 返回-1
        if(m==2) { //2月份的闰平规律测算后确认返回28或29
            return (((y%4 == 0) && (y%100 != 0)) || (y%400 == 0)) ? 29: 28;
        }else {
            return solarMonth[m - 1];
        }
    }

    /**
     * 农历年份转换为干支纪年
     * @param  lYear 农历年的年份数
     * @return Cn string
     */
    QString toGanZhiYear(int lYear) {
        int ganKey = (lYear - 4) % 10;
        int zhiKey = (lYear - 4) % 12;
        return 天干[ganKey] + 地支[zhiKey];        
    }

    /**
      * 传入offset偏移量返回干支
      * @param offset 相对甲子的偏移量
      * @return Cn string
      */
    QString toGanZhi(int offset) {
        return 天干[offset%10] + 地支[offset%12];
    }

    /**
      * 传入公历(!)y年获得该年第n个节气的公历日期
      * @param y公历年(1900-2100)；n二十四节气中的第几个节气(1~24)；从n=1(小寒)算起 
      * @return day Number
      * @eg:var _24 = calendar.getTerm(1987,3) ;//_24=4;意即1987年2月4日立春
      */
    int getTerm(int y, int n) {
        if(y<1900 || y>2100) {return -1;}
        if(n<1 || n>24) {return -1;}
        auto _table = sTermInfo[y-1900];
        QStringList _info = {
            QString::number(QString("0x"+_table.mid(0,5)).toInt()),
            QString::number(QString("0x"+_table.mid(5,5)).toInt()),
            QString::number(QString("0x"+_table.mid(10,5)).toInt()),
            QString::number(QString("0x"+_table.mid(15,5)).toInt()),
            QString::number(QString("0x"+_table.mid(20,5)).toInt()),
            QString::number(QString("0x"+_table.mid(25,5)).toInt())
        };
        QStringList _calday = {
            _info[0].mid(0,1),
            _info[0].mid(1,2),
            _info[0].mid(3,1),
            _info[0].mid(4,2),
            
            _info[1].mid(0,1),
            _info[1].mid(1,2),
            _info[1].mid(3,1),
            _info[1].mid(4,2),
            
            _info[2].mid(0,1),
            _info[2].mid(1,2),
            _info[2].mid(3,1),
            _info[2].mid(4,2),
            
            _info[3].mid(0,1),
            _info[3].mid(1,2),
            _info[3].mid(3,1),
            _info[3].mid(4,2),
            
            _info[4].mid(0,1),
            _info[4].mid(1,2),
            _info[4].mid(3,1),
            _info[4].mid(4,2),
            
            _info[5].mid(0,1),
            _info[5].mid(1,2),
            _info[5].mid(3,1),
            _info[5].mid(4,2),
        };
        return _calday[n-1].toInt();
    }

    /**
      * 传入农历数字月份返回汉语通俗表示法
      * @param lunar month
      * @return Cn string
      * @eg:var cnMonth = calendar.toChinaMonth(12) ;//cnMonth="腊月"
    */
    QString toChinaMonth (int m) {
        if(m>12 || m<1) 
        {
            return nullptr;
        }
        auto s = 月[m-1];
        s+= "月";//加上月字
        return s;
    }
 
    /**
      * 传入农历日期数字返回汉字表示法
      * @param lunar day
      * @return Cn string
      * @eg:var cnDay = calendar.toChinaDay(21) ;//cnMonth="廿一"
      */
    QString toChinaDay(int d){ //日 => \u65e5
        QString s;
        switch (d) {
            case 10:
            s = "\u521d\u5341"; break;
        case 20:
            s = "\u4e8c\u5341"; break;
            break;
        case 30:
            s = "\u4e09\u5341"; break;
            break;
        default :
            s = 十[floor(d/10)];
            s += 日[d%10];
        }
        return(s);
    }
 
    /**
      * 年份转生肖[!仅能大致转换] => 精确划分生肖分界线是“立春”
      * @param y year
      * @return Cn string
      * @eg:var animal = calendar.getAnimal(1987) ;//animal="兔"
      */
    QString getAnimal(int y) {
        return 生肖[(y - 4) % 12];
    }

 public:

    /**
     * 公历年份转换为干支纪年
     * @param  lYear 公历年的年份数
     * @return Cn string
     */
    QString toGanZhiBySolarYear(int year) {
        int ganKey = (year - 4) % 10;
        int zhiKey = (year - 4) % 12;
        return 天干[ganKey] + 地支[zhiKey];        
    }

    QString toDizhiHour(int hour)
    {
        return 地支[(hour + 1) / 2 % 12];
    }
 
    /**
      * 传入阳历年月日获得详细的公历、农历object信息 <=>JSON
      * @param y  solar year
      * @param m  solar month
      * @param d  solar day
      * @return JSON object
      * @eg:console.log(calendar.solar2lunar(1987,11,01));
      */
    QMap<QVariant, QVariant> solar2lunar (const int y, const int m, const int d, const int hour=0) 
    {
         //参数区间1900.1.31~2100.12.31
        QMap<QVariant, QVariant> a; 

        QDate objDate(y, m, d);
        QDate epochDate(1900, 1, 31);
        
        auto i=0, temp=0;
        auto offset = (QDateTime(objDate).toMSecsSinceEpoch() - QDateTime(epochDate).toMSecsSinceEpoch())/86400000;
        for(i=1900; i<2101 && offset>0; ) 
        {
            temp = lYearDays(i);
            if(offset > temp)
            {
                offset -= temp;
                i++;
            }
            else
            {
                break;
            }                        
        }

        //星期几
        auto nWeek = objDate.dayOfWeek();
        QString cWeek  = 日[nWeek];

        //农历年
        auto year = i;
        auto leap = getLeapMonth(i); //闰哪个月
        auto isLeap = false;
        
        //效验闰月
        for(i=1; i<13 && offset>0; i++) {
            //闰月
            if(leap>0 && i==(leap+1) && isLeap==false)
            { 
                --i;
                isLeap = true; 
                temp = leapDays(year); //计算农历闰月天数
            }
            else
            {
                temp = monthDays(year, i);//计算农历普通月天数
            }
            //解除闰月
            if(isLeap==true && i==(leap+1))
            {
                isLeap = false; 
            }
            offset -= temp;
        }
        // 闰月导致数组下标重叠取反
        if(offset==0 && leap>0 && i==leap+1)
        {
            if(isLeap){
                isLeap = false;
            }else{ 
                isLeap = true; 
                --i;
            }
        }

        if(offset<0)
        {
            offset += temp; 
            --i;
        }
        //农历月
        auto month      = i;
        //农历日
        auto day        = offset + 1;
        
        // 当月的两个节气
        // bugfix-2017-7-24 11:03:38 use lunar Year Param `y` Not `year`
        auto firstNode  = getTerm(y, m * 2 - 1);//返回当月「节」为几日开始
        auto secondNode = getTerm(y, m * 2);//返回当月「节」为几日开始
 
        //传入的日期的节气与否
        auto isTerm = false;
        QString Term;
        if(firstNode == d) {
            isTerm  = true;
            Term    = 节气[m*2-2];
        }
        if(secondNode == d) {
            isTerm  = true;
            Term    = 节气[m*2-1];
        }
        //日柱 当月一日与 1900/1/1 相差天数
        int dayCyclical = QDateTime(QDate(y, m - 1, 1)).toMSecsSinceEpoch()/86400000+25567+10;        
        
        a.insert("lYear", year);
        a.insert("lMonth", month);
        a.insert("lDay", day);
        a.insert("animal", getAnimal(year));
        a.insert("ImonthCn", (isLeap ? "润": "") + toChinaMonth(month));
        a.insert("IdayCn", toChinaDay(day));
        a.insert("cYear", y);
        a.insert("cMonth", m);
        a.insert("cDay", d);
        a.insert("gzYear", toGanZhiYear(year));
        a.insert("gzMonth", d>=firstNode ? toGanZhi((y-1900)*12+m+12) : toGanZhi((y-1900)*12+m+11));
        a.insert("gzDay", toGanZhi(dayCyclical + d - 1));
        a.insert("gzHour", toDizhiHour(hour));
        a.insert("isLeap", isLeap);
        a.insert("nWeek", nWeek);
        a.insert("ncWeek", "星期"+cWeek);
        a.insert("isTerm", isTerm);
        a.insert("Term", Term);
        return a;
    }
 
    /**
      * 传入农历年月日以及传入的月份是否闰月获得详细的公历、农历object信息 <=>JSON
      * @param y  lunar year
      * @param m  lunar month
      * @param d  lunar day
      * @param isLeapMonth  lunar month is leap or not.[如果是农历闰月第四个参数赋值true即可]
      * @return JSON object
      * @eg:console.log(calendar.lunar2solar(1987,9,10));
      */
    QMap<QVariant, QVariant> lunar2solar (int y, int m, int d, bool isLeapMonth) {
        auto leapMonth   = getLeapMonth(y);
        QMap<QVariant, QVariant> aa;
        if(isLeapMonth && (leapMonth!=m)) {
            return aa;
        }
        //传参要求计算该闰月公历 但该年得出的闰月与传参的月份并不同
        if((y==2100 && m==12 && d>1) || (y==1900 && m==1 && d<31)) {
            return aa;
        }//超出了最大极限值 
        auto day  = monthDays(y,m); 
        auto _day = day;
        //bugFix 2016-9-25 
        //if month is leap, _day use leapDays method 
        if(isLeapMonth) {
            _day = leapDays(y);
        }
        if(y < 1900 || y > 2100 || d > _day) {
            return aa;
        }//参数合法性效验
        
        //计算农历的时间差
        auto offset = 0;
        for(auto i=1900;i<y;i++) {
            offset+=lYearDays(i);
        }
        auto leap = 0;
        auto isAdd= false;
        for(auto i=1;i<m;i++) 
        {
            leap = getLeapMonth(y);
            if(!isAdd) {//处理闰月
                if(leap<=i && leap>0) {
                    offset+=leapDays(y);
                    isAdd = true;
                }
            }
            offset+=monthDays(y,i);
        }
        //转换闰月农历 需补充该年闰月的前一个月的时差
        if(isLeapMonth) {
            offset+=day;
        }
        //1900年农历正月一日的公历时间为1900年1月30日0时0分0秒(该时间也是本农历的最开始起始点)
        auto stmap   =   QDateTime(QDate(1900,1,30)).toMSecsSinceEpoch();
        auto calObj  =   QDateTime::fromMSecsSinceEpoch((offset+d-31)*86400000+stmap);
        auto cY      =   calObj.date().year();
        auto cM      =   calObj.date().month()+1;
        auto cD      =   calObj.date().day();
        return solar2lunar(cY,cM,cD);
    }
};


DWIDGET_USE_NAMESPACE

DatetimeWidget::DatetimeWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(PLUGIN_BACKGROUND_MIN_SIZE, PLUGIN_BACKGROUND_MIN_SIZE);
}

void DatetimeWidget::set24HourFormat(const bool value)
{
    if (m_24HourFormat == value) {
        return;
    }

    m_24HourFormat = value;
    update();

    adjustSize();
    if (isVisible()) {
        emit requestUpdateGeometry();
    }
}

QSize DatetimeWidget::curTimeSize() const
{
    QFontMetrics fm(TIME_FONT);
    return QSize(fm.boundingRect(currentChinaTime()).size().width() - 120, height());
}

QSize DatetimeWidget::sizeHint() const
{
    return curTimeSize();
}

void DatetimeWidget::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setFont(m_timeFont);
    painter.setPen(QPen(palette().brightText(), 1));

    painter.drawText(rect(), Qt::AlignVCenter | Qt::AlignHCenter, currentChinaTime());
}

QString DatetimeWidget::currentChinaTime() const
{
    const QDateTime current = QDateTime::currentDateTime();
    Lunar lunar;    
    QString format = m_24HourFormat ? "yyyy/MM/dd  hh:mm %1年  %2时" : "yyyy/MM/dd hh:mm AP %1年  %2时";

    return current.toString(format).arg(lunar.toGanZhiBySolarYear(current.date().year())).arg(lunar.toDizhiHour(current.time().hour()));
}

QStringList DatetimeWidget::dateString()
{
    QDateTime current = QDateTime::currentDateTime();
    Lunar lunar;
    QMap<QVariant, QVariant> dd = lunar.solar2lunar(current.date().year(), current.date().month(), current.date().day(), current.time().hour());

    QStringList tips;
    tips.append(QString("天地：%1年 %2月 %3日 %4时").arg(dd.value("gzYear").toString(), dd.value("gzMonth").toString(), dd.value("gzDay").toString(), dd.value("gzHour").toString()));
    tips.append(QString("农历：%1年%2%3 %4 %5").arg(dd.value("lYear").toString(), dd.value("ImonthCn").toString(), dd.value("IdayCn").toString(), dd.value("animal").toString(), dd.value("Term").toString()));

    return tips;
}