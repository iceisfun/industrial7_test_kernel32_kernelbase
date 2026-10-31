/* GetLocaleInfoEx / GetLocaleInfoW / GetLocaleInfoA value tables for fixed locales.
 * "R" rows are hard expectations; "S" rows are values we believe but cannot verify
 * against the decompilation (locale data lives in locale.nls) - they are only noted. */
#include "nlsutil.h"

typedef struct { DWORD type; const WCHAR *val; int hard; } row_t;
#define R(t, v) { t, v, 1 }
#define S(t, v) { t, v, 0 }

static void run_rows(const WCHAR *loc, const row_t *rows, int n)
{
    REQUIRE(U(GetLocaleInfoEx));
    REQUIRE(U(GetLocaleInfoW));
    REQUIRE(U(LocaleNameToLCID));
    WCHAR buf[512];
    LCID lcid = U(LocaleNameToLCID)(loc, 0);
    CHECK(lcid != 0);
    for (int i = 0; i < n; i++) {
        SET_ERR(0);
        int r = nls_str(loc, rows[i].type, buf, 512);
        if (r <= 0) { CHECKF(r > 0, "type 0x%lx err %lu", rows[i].type, GetLastError()); continue; }
        CHECKF(r == (int)lstrlenW(buf) + 1, "type 0x%lx r=%d len=%d", rows[i].type, r, lstrlenW(buf));
        if (rows[i].type == L_ILANGUAGE || rows[i].type == L_IDEFAULTLANGUAGE) { if (!lstrcmpiW(buf, rows[i].val)) CHECK(1); else EXPECT_WS(rows[i].hard, loc, "value", buf, rows[i].val); }
        else EXPECT_WS(rows[i].hard, loc, "value", buf, rows[i].val);
        /* GetLocaleInfoW with the LCID must agree with GetLocaleInfoEx. */
        WCHAR b2[512];
        int r2 = U(GetLocaleInfoW)(lcid, rows[i].type | N_NOUSER, b2, 512);
        CHECKF(r2 == r && !lstrcmpW(b2, buf), "W vs Ex type 0x%lx", rows[i].type);
    }
}

#define TAB(tag, nm) \
    TEST(locinfo_table_##tag) { run_rows(nm, rows_##tag, ARRAYLEN(rows_##tag)); }

static const row_t rows_en_US[] = {
    R(L_SNAME, L"en-US"), R(L_ILANGUAGE, L"0409"), R(L_IDEFAULTLANGUAGE, L"0409"),
    R(L_SENGLANGNAME, L"English"), R(L_SABBREVLANGNAME, L"ENU"), R(L_SISO639LANGNAME, L"en"),
    R(L_SISO639LANGNAME2, L"eng"), R(L_SISO3166CTRYNAME, L"US"), R(L_SISO3166CTRYNAME2, L"USA"),
    R(L_SENGCOUNTRY, L"United States"), R(L_SABBREVCTRYNAME, L"USA"), R(L_ICOUNTRY, L"1"),
    R(L_IDEFAULTCOUNTRY, L"1"),
    R(L_SINTLSYMBOL, L"USD"), R(L_SCURRENCY, L"$"), R(L_SENGCURRNAME, L"US Dollar"),
    R(L_IDEFAULTANSICP, L"1252"), R(L_IDEFAULTCODEPAGE, L"437"), R(L_IDEFAULTMACCP, L"10000"),
    R(L_IDEFAULTEBCDICCP, L"037"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_SGROUPING, L"3;0"), R(L_SMONDECIMALSEP, L"."),
    R(L_SMONTHOUSANDSEP, L","), R(L_SMONGROUPING, L"3;0"), R(L_SPOSITIVESIGN, L""), R(L_SNEGATIVESIGN, L"-"),
    R(L_SLIST, L","), R(L_IMEASURE, L"1"), R(L_IDIGITS, L"2"), R(L_ILZERO, L"1"), R(L_INEGNUMBER, L"1"),
    R(L_ICURRDIGITS, L"2"), R(L_ICURRENCY, L"0"), R(L_INEGCURR, L"0"),
    R(L_SDATE, L"/"), R(L_STIME, L":"), R(L_SSHORTDATE, L"M/d/yyyy"), R(L_SLONGDATE, L"dddd, MMMM d, yyyy"),
    R(L_STIMEFORMAT, L"h:mm:ss tt"), R(L_SYEARMONTH, L"MMMM yyyy"),
    R(L_IDATE, L"0"), R(L_ILDATE, L"0"), R(L_ITIME, L"0"), R(L_ITIMEMARKPOSN, L"0"), R(L_ITLZERO, L"0"),
    R(L_ICALENDARTYPE, L"1"), R(L_IOPTIONALCALENDAR, L"0"),
    R(L_S1159, L"AM"), R(L_S2359, L"PM"),
    R(L_SDAYNAME1, L"Monday"), R(L_SDAYNAME1 + 1, L"Tuesday"), R(L_SDAYNAME1 + 2, L"Wednesday"),
    R(L_SDAYNAME1 + 3, L"Thursday"), R(L_SDAYNAME1 + 4, L"Friday"), R(L_SDAYNAME1 + 5, L"Saturday"),
    R(L_SDAYNAME1 + 6, L"Sunday"),
    R(L_SABBREVDAYNAME1, L"Mon"), R(L_SABBREVDAYNAME1 + 1, L"Tue"), R(L_SABBREVDAYNAME1 + 2, L"Wed"),
    R(L_SABBREVDAYNAME1 + 3, L"Thu"), R(L_SABBREVDAYNAME1 + 4, L"Fri"), R(L_SABBREVDAYNAME1 + 5, L"Sat"),
    R(L_SABBREVDAYNAME1 + 6, L"Sun"),
    R(L_SMONTHNAME1, L"January"), R(L_SMONTHNAME1 + 1, L"February"), R(L_SMONTHNAME1 + 2, L"March"),
    R(L_SMONTHNAME1 + 3, L"April"), R(L_SMONTHNAME1 + 4, L"May"), R(L_SMONTHNAME1 + 5, L"June"),
    R(L_SMONTHNAME1 + 6, L"July"), R(L_SMONTHNAME1 + 7, L"August"), R(L_SMONTHNAME1 + 8, L"September"),
    R(L_SMONTHNAME1 + 9, L"October"), R(L_SMONTHNAME1 + 10, L"November"), R(L_SMONTHNAME1 + 11, L"December"),
    R(L_SMONTHNAME13, L""),
    R(L_SABBREVMONTHNAME1, L"Jan"), R(L_SABBREVMONTHNAME1 + 1, L"Feb"), R(L_SABBREVMONTHNAME1 + 2, L"Mar"),
    R(L_SABBREVMONTHNAME1 + 3, L"Apr"), R(L_SABBREVMONTHNAME1 + 4, L"May"), R(L_SABBREVMONTHNAME1 + 5, L"Jun"),
    R(L_SABBREVMONTHNAME1 + 6, L"Jul"), R(L_SABBREVMONTHNAME1 + 7, L"Aug"), R(L_SABBREVMONTHNAME1 + 8, L"Sep"),
    R(L_SABBREVMONTHNAME1 + 9, L"Oct"), R(L_SABBREVMONTHNAME1 + 10, L"Nov"), R(L_SABBREVMONTHNAME1 + 11, L"Dec"),
    R(L_SABBREVMONTHNAME13, L""),
    R(L_SNATIVELANGNAME, L"English"), R(L_SNATIVECTRYNAME, L"United States"), R(L_SPARENT, L"en"),
    R(L_SCONSOLEFALLBACKNAME, L"en-US"), R(L_IGEOID, L"244"),
    S(L_SSHORTESTDAYNAME1, L"Mo"), S(L_SSHORTESTDAYNAME1 + 6, L"Su"),
    S(L_SNAN, L"NaN"), S(L_SPOSINFINITY, L"∞"), S(L_SNEGINFINITY, L"-∞"),
    S(L_SENGDISPLAYNAME, L"English (United States)"), S(L_SNATDISPLAYNAME, L"English (United States)"),
    S(L_SLANGUAGE, L"English (United States)"), S(L_SCOUNTRY, L"United States"),
    S(L_SSORTNAME, L"Default"), S(L_SSCRIPTS, L"Latn;"), S(L_SSHORTTIME, L"h:mm tt"),
    S(L_SNATIVECURRNAME, L"US Dollar"), S(L_SDURATION, L"h:mm:ss"),
    S(L_IFIRSTDAYOFWEEK, L"6"), S(L_IFIRSTWEEKOFYEAR, L"0"), S(L_IPAPERSIZE, L"1"),
    S(L_IREADINGLAYOUT, L"0"), S(L_INEUTRAL, L"0"), S(L_IDIGITSUBSTITUTION, L"1"),
    S(L_SNATIVEDIGITS, L"0123456789"), S(L_SOPENTYPELANGTAG, L"ENG"), S(L_SKEYBOARDSTOINSTALL, L"00000409"),
    S(L_SSHORTESTAM, L"A"), S(L_SSHORTESTPM, L"P"), S(L_ICENTURY, L"1"), S(L_IDAYLZERO, L"0"), S(L_IMONLZERO, L"0"),
    S(L_IPOSSIGNPOSN, L"3"), S(L_INEGSIGNPOSN, L"0"), S(L_IPOSSYMPRECEDES, L"1"), S(L_INEGSYMPRECEDES, L"1"),
};
TAB(en_US, L"en-US")

static const row_t rows_de_DE[] = {
    R(L_SNAME, L"de-DE"), R(L_ILANGUAGE, L"0407"), R(L_SENGLANGNAME, L"German"), R(L_SABBREVLANGNAME, L"DEU"),
    R(L_SISO639LANGNAME, L"de"), R(L_SISO3166CTRYNAME, L"DE"), R(L_SISO3166CTRYNAME2, L"DEU"),
    R(L_ICOUNTRY, L"49"), R(L_SINTLSYMBOL, L"EUR"), R(L_SCURRENCY, L"€"),
    R(L_IDEFAULTANSICP, L"1252"), R(L_IDEFAULTCODEPAGE, L"850"),
    R(L_SDECIMAL, L","), R(L_STHOUSAND, L"."), R(L_SGROUPING, L"3;0"), R(L_SMONDECIMALSEP, L","), R(L_SMONTHOUSANDSEP, L"."),
    R(L_IMEASURE, L"0"), R(L_SDATE, L"."), R(L_STIME, L":"), S(L_SSHORTDATE, L"dd.MM.yyyy"),
    R(L_SLONGDATE, L"dddd, d. MMMM yyyy"), R(L_STIMEFORMAT, L"HH:mm:ss"), R(L_SENGCOUNTRY, L"Germany"),
    R(L_SDAYNAME1, L"Montag"), R(L_SDAYNAME1 + 1, L"Dienstag"), R(L_SDAYNAME1 + 2, L"Mittwoch"),
    R(L_SDAYNAME1 + 3, L"Donnerstag"), R(L_SDAYNAME1 + 4, L"Freitag"), R(L_SDAYNAME1 + 5, L"Samstag"),
    R(L_SDAYNAME1 + 6, L"Sonntag"),
    R(L_SMONTHNAME1, L"Januar"), R(L_SMONTHNAME1 + 1, L"Februar"), R(L_SMONTHNAME1 + 2, L"März"),
    R(L_SMONTHNAME1 + 3, L"April"), R(L_SMONTHNAME1 + 4, L"Mai"), R(L_SMONTHNAME1 + 5, L"Juni"),
    R(L_SMONTHNAME1 + 6, L"Juli"), R(L_SMONTHNAME1 + 7, L"August"), R(L_SMONTHNAME1 + 8, L"September"),
    R(L_SMONTHNAME1 + 9, L"Oktober"), R(L_SMONTHNAME1 + 10, L"November"), R(L_SMONTHNAME1 + 11, L"Dezember"),
    R(L_SNATIVELANGNAME, L"Deutsch"), R(L_SPARENT, L"de"), R(L_IGEOID, L"94"), R(L_SLIST, L";"),
    S(L_SABBREVDAYNAME1, L"Mo"), S(L_SABBREVDAYNAME1 + 1, L"Di"), S(L_SABBREVDAYNAME1 + 2, L"Mi"),
    S(L_SABBREVDAYNAME1 + 3, L"Do"), S(L_SABBREVDAYNAME1 + 4, L"Fr"), S(L_SABBREVDAYNAME1 + 5, L"Sa"),
    S(L_SABBREVDAYNAME1 + 6, L"So"),
    S(L_SABBREVMONTHNAME1, L"Jan"), S(L_SABBREVMONTHNAME1 + 2, L"Mär"), S(L_SABBREVMONTHNAME1 + 9, L"Okt"),
    S(L_SABBREVMONTHNAME1 + 11, L"Dez"),
    S(L_S1159, L""), S(L_S2359, L""), S(L_SNATIVECTRYNAME, L"Deutschland"), S(L_IPAPERSIZE, L"9"),
    S(L_SYEARMONTH, L"MMMM yyyy"), S(L_SENGCURRNAME, L"Euro"), S(L_SNATIVECURRNAME, L"Euro"),
    S(L_IFIRSTDAYOFWEEK, L"0"), S(L_IFIRSTWEEKOFYEAR, L"2"), S(L_SSHORTTIME, L"HH:mm"),
};
TAB(de_DE, L"de-DE")

static const row_t rows_fr_FR[] = {
    R(L_SNAME, L"fr-FR"), R(L_ILANGUAGE, L"040C"), R(L_SENGLANGNAME, L"French"), R(L_SABBREVLANGNAME, L"FRA"),
    R(L_SISO639LANGNAME, L"fr"), R(L_SISO3166CTRYNAME, L"FR"), R(L_SISO3166CTRYNAME2, L"FRA"),
    R(L_ICOUNTRY, L"33"), R(L_SINTLSYMBOL, L"EUR"), R(L_SCURRENCY, L"€"),
    R(L_IDEFAULTANSICP, L"1252"), R(L_IDEFAULTCODEPAGE, L"850"), R(L_SDECIMAL, L","), R(L_IMEASURE, L"0"),
    R(L_SDATE, L"/"), R(L_STIME, L":"), S(L_SSHORTDATE, L"dd/MM/yyyy"), R(L_SLONGDATE, L"dddd d MMMM yyyy"),
    R(L_STIMEFORMAT, L"HH:mm:ss"), R(L_SLIST, L";"),
    R(L_SDAYNAME1, L"lundi"), R(L_SDAYNAME1 + 1, L"mardi"), R(L_SDAYNAME1 + 2, L"mercredi"),
    R(L_SDAYNAME1 + 3, L"jeudi"), R(L_SDAYNAME1 + 4, L"vendredi"), R(L_SDAYNAME1 + 5, L"samedi"),
    R(L_SDAYNAME1 + 6, L"dimanche"),
    R(L_SMONTHNAME1, L"janvier"), R(L_SMONTHNAME1 + 1, L"février"), R(L_SMONTHNAME1 + 2, L"mars"),
    R(L_SMONTHNAME1 + 3, L"avril"), R(L_SMONTHNAME1 + 4, L"mai"), R(L_SMONTHNAME1 + 5, L"juin"),
    R(L_SMONTHNAME1 + 6, L"juillet"), R(L_SMONTHNAME1 + 7, L"août"), R(L_SMONTHNAME1 + 8, L"septembre"),
    R(L_SMONTHNAME1 + 9, L"octobre"), R(L_SMONTHNAME1 + 10, L"novembre"), R(L_SMONTHNAME1 + 11, L"décembre"),
    R(L_SNATIVELANGNAME, L"français"), R(L_SPARENT, L"fr"), R(L_IGEOID, L"84"),
    S(L_STHOUSAND, L" "), S(L_SGROUPING, L"3;0"), S(L_SABBREVMONTHNAME1, L"janv."),
    S(L_SABBREVMONTHNAME1 + 1, L"févr."), S(L_SABBREVDAYNAME1, L"lun."), S(L_SNATIVECTRYNAME, L"France"),
    S(L_S1159, L""), S(L_IPAPERSIZE, L"9"), S(L_IFIRSTDAYOFWEEK, L"0"),
};
TAB(fr_FR, L"fr-FR")

static const row_t rows_ja_JP[] = {
    R(L_SNAME, L"ja-JP"), R(L_ILANGUAGE, L"0411"), R(L_SENGLANGNAME, L"Japanese"), R(L_SABBREVLANGNAME, L"JPN"),
    R(L_SISO639LANGNAME, L"ja"), R(L_SISO3166CTRYNAME, L"JP"), R(L_SISO3166CTRYNAME2, L"JPN"),
    R(L_ICOUNTRY, L"81"), R(L_SINTLSYMBOL, L"JPY"), R(L_IDEFAULTANSICP, L"932"), R(L_IDEFAULTCODEPAGE, L"932"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_IMEASURE, L"0"), R(L_SDATE, L"/"), R(L_STIME, L":"),
    S(L_SSHORTDATE, L"yyyy/MM/dd"), S(L_SLONGDATE, L"yyyy'年'M'月'd'日'"), S(L_STIMEFORMAT, L"H:mm:ss"),
    R(L_SDAYNAME1, L"月曜日"), R(L_SDAYNAME1 + 1, L"火曜日"), R(L_SDAYNAME1 + 2, L"水曜日"),
    R(L_SDAYNAME1 + 3, L"木曜日"), R(L_SDAYNAME1 + 4, L"金曜日"), R(L_SDAYNAME1 + 5, L"土曜日"),
    R(L_SDAYNAME1 + 6, L"日曜日"),
    R(L_SMONTHNAME1, L"1月"), R(L_SMONTHNAME1 + 5, L"6月"), R(L_SMONTHNAME1 + 11, L"12月"),
    R(L_SNATIVELANGNAME, L"日本語"), R(L_IGEOID, L"122"),
    S(L_S1159, L"午前"), S(L_S2359, L"午後"), S(L_SABBREVDAYNAME1, L"月"),
    S(L_SCURRENCY, L"￥"), S(L_IPAPERSIZE, L"9"), S(L_SLIST, L","), S(L_ICALENDARTYPE, L"1"),
    S(L_IOPTIONALCALENDAR, L"3"), S(L_SNATIVECTRYNAME, L"日本"), S(L_IFIRSTDAYOFWEEK, L"6"),
};
TAB(ja_JP, L"ja-JP")

static const row_t rows_zh_CN[] = {
    R(L_SNAME, L"zh-CN"), R(L_ILANGUAGE, L"0804"), R(L_SABBREVLANGNAME, L"CHS"), R(L_SISO639LANGNAME, L"zh"),
    R(L_SISO3166CTRYNAME, L"CN"), R(L_SISO3166CTRYNAME2, L"CHN"), R(L_ICOUNTRY, L"86"), R(L_SINTLSYMBOL, L"CNY"),
    R(L_IDEFAULTANSICP, L"936"), R(L_IDEFAULTCODEPAGE, L"936"), R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","),
    R(L_IMEASURE, L"0"), S(L_SSHORTDATE, L"yyyy/M/d"), S(L_SLONGDATE, L"yyyy'年'M'月'd'日'"),
    S(L_STIMEFORMAT, L"H:mm:ss"),
    R(L_SDAYNAME1, L"星期一"), R(L_SDAYNAME1 + 1, L"星期二"), R(L_SDAYNAME1 + 2, L"星期三"),
    R(L_SDAYNAME1 + 3, L"星期四"), R(L_SDAYNAME1 + 4, L"星期五"), R(L_SDAYNAME1 + 5, L"星期六"),
    R(L_SDAYNAME1 + 6, L"星期日"),
    R(L_SMONTHNAME1, L"一月"), R(L_SMONTHNAME1 + 1, L"二月"), R(L_SMONTHNAME1 + 11, L"十二月"),
    R(L_IGEOID, L"45"),
    S(L_S1159, L"上午"), S(L_S2359, L"下午"), S(L_SPARENT, L"zh-Hans"), S(L_SNATIVELANGNAME, L"中文"),
    S(L_SCURRENCY, L"￥"), S(L_IPAPERSIZE, L"9"), S(L_SLIST, L","), S(L_IFIRSTDAYOFWEEK, L"0"),
    S(L_SENGLANGNAME, L"Chinese"),
};
TAB(zh_CN, L"zh-CN")

static const row_t rows_ar_SA[] = {
    R(L_SNAME, L"ar-SA"), R(L_ILANGUAGE, L"0401"), R(L_SENGLANGNAME, L"Arabic"), R(L_SABBREVLANGNAME, L"ARA"),
    R(L_SISO639LANGNAME, L"ar"), R(L_SISO3166CTRYNAME, L"SA"), R(L_SISO3166CTRYNAME2, L"SAU"), R(L_ICOUNTRY, L"966"),
    R(L_SINTLSYMBOL, L"SAR"), R(L_IDEFAULTANSICP, L"1256"), R(L_IDEFAULTCODEPAGE, L"720"), R(L_IGEOID, L"205"),
    S(L_SDECIMAL, L"."), S(L_STHOUSAND, L","), S(L_ICALENDARTYPE, L"6"), S(L_IMEASURE, L"0"),
    S(L_SNATIVELANGNAME, L"العربية"), S(L_SPARENT, L"ar"), S(L_SLIST, L";"),
    S(L_IFIRSTDAYOFWEEK, L"5"), S(L_IPAPERSIZE, L"9"), S(L_SSHORTDATE, L"dd/MM/yy"),
};
TAB(ar_SA, L"ar-SA")

static const row_t rows_tr_TR[] = {
    R(L_SNAME, L"tr-TR"), R(L_ILANGUAGE, L"041F"), R(L_SENGLANGNAME, L"Turkish"), R(L_SABBREVLANGNAME, L"TRK"),
    R(L_SISO639LANGNAME, L"tr"), R(L_SISO3166CTRYNAME, L"TR"), R(L_SISO3166CTRYNAME2, L"TUR"), R(L_ICOUNTRY, L"90"),
    R(L_SINTLSYMBOL, L"TRY"), R(L_IDEFAULTANSICP, L"1254"), R(L_IDEFAULTCODEPAGE, L"857"), R(L_SDECIMAL, L","),
    R(L_STHOUSAND, L"."), R(L_IMEASURE, L"0"), R(L_SDATE, L"."), R(L_STIME, L":"), R(L_IGEOID, L"235"),
    R(L_SDAYNAME1, L"Pazartesi"), R(L_SDAYNAME1 + 1, L"Salı"), R(L_SDAYNAME1 + 2, L"Çarşamba"),
    R(L_SDAYNAME1 + 3, L"Perşembe"), R(L_SDAYNAME1 + 4, L"Cuma"), R(L_SDAYNAME1 + 5, L"Cumartesi"),
    R(L_SDAYNAME1 + 6, L"Pazar"),
    R(L_SMONTHNAME1, L"Ocak"), R(L_SMONTHNAME1 + 1, L"Şubat"), R(L_SMONTHNAME1 + 2, L"Mart"),
    R(L_SMONTHNAME1 + 3, L"Nisan"), R(L_SMONTHNAME1 + 4, L"Mayıs"), R(L_SMONTHNAME1 + 5, L"Haziran"),
    R(L_SMONTHNAME1 + 6, L"Temmuz"), R(L_SMONTHNAME1 + 7, L"Ağustos"), R(L_SMONTHNAME1 + 8, L"Eylül"),
    R(L_SMONTHNAME1 + 9, L"Ekim"), R(L_SMONTHNAME1 + 10, L"Kasım"), R(L_SMONTHNAME1 + 11, L"Aralık"),
    S(L_SSHORTDATE, L"d.MM.yyyy"), S(L_SLONGDATE, L"d MMMM yyyy dddd"), S(L_STIMEFORMAT, L"HH:mm:ss"),
    S(L_SNATIVELANGNAME, L"Türkçe"), S(L_SLIST, L";"), S(L_IPAPERSIZE, L"9"), S(L_SCURRENCY, L"₺"),
};
TAB(tr_TR, L"tr-TR")

static const row_t rows_he_IL[] = {
    R(L_SNAME, L"he-IL"), R(L_ILANGUAGE, L"040D"), R(L_SENGLANGNAME, L"Hebrew"), R(L_SABBREVLANGNAME, L"HEB"),
    R(L_SISO639LANGNAME, L"he"), R(L_SISO3166CTRYNAME, L"IL"), R(L_SISO3166CTRYNAME2, L"ISR"), R(L_ICOUNTRY, L"972"),
    R(L_SINTLSYMBOL, L"ILS"), R(L_IDEFAULTANSICP, L"1255"), R(L_IDEFAULTCODEPAGE, L"862"), R(L_IGEOID, L"117"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_IMEASURE, L"0"),
    S(L_SSHORTDATE, L"dd/MM/yyyy"), S(L_STIMEFORMAT, L"HH:mm:ss"), S(L_SCURRENCY, L"₪"),
    S(L_SNATIVELANGNAME, L"עברית"), S(L_SLIST, L","), S(L_IFIRSTDAYOFWEEK, L"6"),
};
TAB(he_IL, L"he-IL")

static const row_t rows_th_TH[] = {
    R(L_SNAME, L"th-TH"), R(L_ILANGUAGE, L"041E"), R(L_SENGLANGNAME, L"Thai"), R(L_SABBREVLANGNAME, L"THA"),
    R(L_SISO639LANGNAME, L"th"), R(L_SISO3166CTRYNAME, L"TH"), R(L_SISO3166CTRYNAME2, L"THA"), R(L_ICOUNTRY, L"66"),
    R(L_SINTLSYMBOL, L"THB"), R(L_IDEFAULTANSICP, L"874"), R(L_IDEFAULTCODEPAGE, L"874"), R(L_IGEOID, L"227"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_IMEASURE, L"0"),
    S(L_ICALENDARTYPE, L"7"), S(L_SSHORTDATE, L"d/M/yyyy"), S(L_STIMEFORMAT, L"H:mm:ss"), S(L_SCURRENCY, L"฿"),
    S(L_SNATIVELANGNAME, L"ไทย"), S(L_IOPTIONALCALENDAR, L"1"), S(L_SLIST, L","),
};
TAB(th_TH, L"th-TH")

static const row_t rows_hi_IN[] = {
    R(L_SNAME, L"hi-IN"), R(L_ILANGUAGE, L"0439"), R(L_SENGLANGNAME, L"Hindi"), R(L_SABBREVLANGNAME, L"HIN"),
    R(L_SISO639LANGNAME, L"hi"), R(L_SISO3166CTRYNAME, L"IN"), R(L_SISO3166CTRYNAME2, L"IND"), R(L_ICOUNTRY, L"91"),
    R(L_SINTLSYMBOL, L"INR"), R(L_IGEOID, L"113"), R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_IMEASURE, L"0"),
    S(L_SGROUPING, L"3;2;0"), S(L_SMONGROUPING, L"3;2;0"), S(L_IDEFAULTANSICP, L"0"), S(L_IDEFAULTCODEPAGE, L"1"),
    S(L_SCURRENCY, L"₹"), S(L_SSHORTDATE, L"dd-MM-yyyy"), S(L_STIMEFORMAT, L"HH:mm:ss"), S(L_SLIST, L","),
};
TAB(hi_IN, L"hi-IN")

static const row_t rows_ko_KR[] = {
    R(L_SNAME, L"ko-KR"), R(L_ILANGUAGE, L"0412"), R(L_SENGLANGNAME, L"Korean"), R(L_SABBREVLANGNAME, L"KOR"),
    R(L_SISO639LANGNAME, L"ko"), R(L_SISO3166CTRYNAME, L"KR"), R(L_SISO3166CTRYNAME2, L"KOR"), R(L_ICOUNTRY, L"82"),
    R(L_SINTLSYMBOL, L"KRW"), R(L_IDEFAULTANSICP, L"949"), R(L_IDEFAULTCODEPAGE, L"949"), R(L_IGEOID, L"134"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_IMEASURE, L"0"),
    R(L_SDAYNAME1, L"월요일"), R(L_SDAYNAME1 + 1, L"화요일"), R(L_SDAYNAME1 + 2, L"수요일"),
    R(L_SDAYNAME1 + 3, L"목요일"), R(L_SDAYNAME1 + 4, L"금요일"), R(L_SDAYNAME1 + 5, L"토요일"),
    R(L_SDAYNAME1 + 6, L"일요일"),
    R(L_SMONTHNAME1, L"1월"), R(L_SMONTHNAME1 + 11, L"12월"),
    S(L_SSHORTDATE, L"yyyy-MM-dd"), S(L_STIMEFORMAT, L"tt h:mm:ss"), S(L_S1159, L"오전"), S(L_S2359, L"오후"),
    S(L_SCURRENCY, L"₩"), S(L_SNATIVELANGNAME, L"한국어"), S(L_SLIST, L","),
};
TAB(ko_KR, L"ko-KR")

static const row_t rows_ru_RU[] = {
    R(L_SNAME, L"ru-RU"), R(L_ILANGUAGE, L"0419"), R(L_SENGLANGNAME, L"Russian"), R(L_SABBREVLANGNAME, L"RUS"),
    R(L_SISO639LANGNAME, L"ru"), R(L_SISO3166CTRYNAME, L"RU"), R(L_SISO3166CTRYNAME2, L"RUS"), R(L_ICOUNTRY, L"7"),
    R(L_SINTLSYMBOL, L"RUB"), R(L_IDEFAULTANSICP, L"1251"), R(L_IDEFAULTCODEPAGE, L"866"), R(L_IGEOID, L"203"),
    R(L_SDECIMAL, L","), R(L_IMEASURE, L"0"), R(L_SDATE, L"."), S(L_SSHORTDATE, L"dd.MM.yyyy"),
    R(L_SDAYNAME1, L"понедельник"),
    R(L_SDAYNAME1 + 1, L"вторник"),
    R(L_SDAYNAME1 + 2, L"среда"),
    R(L_SDAYNAME1 + 3, L"четверг"),
    R(L_SDAYNAME1 + 4, L"пятница"),
    R(L_SDAYNAME1 + 5, L"суббота"),
    R(L_SDAYNAME1 + 6, L"воскресенье"),
    R(L_SMONTHNAME1, L"январь"), R(L_SMONTHNAME1 + 2, L"март"),
    R(L_SMONTHNAME1 + 4, L"май"),
    S(L_STHOUSAND, L" "), S(L_SLIST, L";"), S(L_STIMEFORMAT, L"H:mm:ss"), S(L_SCURRENCY, L"₽"),
    S(L_SNATIVELANGNAME, L"русский"), S(L_SLONGDATE, L"d MMMM yyyy 'г'."),
};
TAB(ru_RU, L"ru-RU")

static const row_t rows_pt_BR[] = {
    R(L_SNAME, L"pt-BR"), R(L_ILANGUAGE, L"0416"), R(L_SENGLANGNAME, L"Portuguese"), R(L_SABBREVLANGNAME, L"PTB"),
    R(L_SISO639LANGNAME, L"pt"), R(L_SISO3166CTRYNAME, L"BR"), R(L_SISO3166CTRYNAME2, L"BRA"), R(L_ICOUNTRY, L"55"),
    R(L_SINTLSYMBOL, L"BRL"), R(L_IDEFAULTANSICP, L"1252"), R(L_IDEFAULTCODEPAGE, L"850"), R(L_IGEOID, L"32"),
    R(L_SDECIMAL, L","), R(L_STHOUSAND, L"."), R(L_IMEASURE, L"0"), R(L_SDATE, L"/"), S(L_SSHORTDATE, L"dd/MM/yyyy"),
    R(L_SDAYNAME1, L"segunda-feira"), R(L_SDAYNAME1 + 1, L"terça-feira"), R(L_SDAYNAME1 + 2, L"quarta-feira"),
    R(L_SDAYNAME1 + 3, L"quinta-feira"), R(L_SDAYNAME1 + 4, L"sexta-feira"), R(L_SDAYNAME1 + 5, L"sábado"),
    R(L_SDAYNAME1 + 6, L"domingo"),
    R(L_SMONTHNAME1, L"janeiro"), R(L_SMONTHNAME1 + 1, L"fevereiro"), R(L_SMONTHNAME1 + 2, L"março"),
    R(L_SMONTHNAME1 + 3, L"abril"), R(L_SMONTHNAME1 + 4, L"maio"), R(L_SMONTHNAME1 + 5, L"junho"),
    R(L_SMONTHNAME1 + 6, L"julho"), R(L_SMONTHNAME1 + 7, L"agosto"), R(L_SMONTHNAME1 + 8, L"setembro"),
    R(L_SMONTHNAME1 + 9, L"outubro"), R(L_SMONTHNAME1 + 10, L"novembro"), R(L_SMONTHNAME1 + 11, L"dezembro"),
    S(L_SLIST, L";"), S(L_STIMEFORMAT, L"HH:mm:ss"), S(L_SLONGDATE, L"dddd, d' de 'MMMM' de 'yyyy"),
    S(L_SCURRENCY, L"R$"), S(L_SNATIVELANGNAME, L"português"), S(L_IPAPERSIZE, L"9"),
};
TAB(pt_BR, L"pt-BR")

/* Invariant locale */
static const row_t rows_inv[] = {
    R(L_SNAME, L""), R(L_ILANGUAGE, L"007F"), R(L_SISO639LANGNAME, L"iv"), R(L_SISO3166CTRYNAME, L"IV"),
    R(L_SABBREVLANGNAME, L"IVL"), R(L_IDEFAULTANSICP, L"1252"), R(L_IDEFAULTCODEPAGE, L"437"),
    R(L_SDECIMAL, L"."), R(L_STHOUSAND, L","), R(L_SGROUPING, L"3;0"), R(L_SLIST, L","),
    R(L_SDATE, L"/"), R(L_STIME, L":"), R(L_SSHORTDATE, L"MM/dd/yyyy"), R(L_SLONGDATE, L"dddd, dd MMMM yyyy"),
    R(L_STIMEFORMAT, L"HH:mm:ss"), R(L_S1159, L"AM"), R(L_S2359, L"PM"), R(L_IMEASURE, L"0"),
    R(L_SDAYNAME1, L"Monday"), R(L_SDAYNAME1 + 6, L"Sunday"), R(L_SMONTHNAME1, L"January"), R(L_SMONTHNAME1 + 11, L"December"),
    R(L_SABBREVDAYNAME1, L"Mon"), R(L_SABBREVMONTHNAME1, L"Jan"),
    S(L_SENGLANGNAME, L"Invariant Language"), S(L_SENGCOUNTRY, L"Invariant Country"), S(L_SINTLSYMBOL, L"XDR"),
    S(L_SCURRENCY, L"¤"), S(L_ICOUNTRY, L"1"), S(L_SYEARMONTH, L"yyyy MMMM"),
};
TAB(inv, L"")
