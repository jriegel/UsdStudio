// (c)2018 Juergen Riegel, Daimler Protics GmbH, Juergen.Riegel@daimler.com


#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <qdatetime.h>
#include <ctime> 
#include <iostream>
#include <string>
#include <sstream>

#include <chrono>

using namespace std;
using namespace chrono;


time_t time_when_compiled();



#include "../Src/Public/mainwindow.h"



int main(int argc, char *argv[])
{
    QApplication Application(argc, argv);
    Q_INIT_RESOURCE(resources);


	// set Style sheet black/orange
	//QFile file(":/stylesheet.qss");
	//file.open(QFile::ReadOnly);
	//QString styleSheet = QLatin1String(file.readAll());
	//Application.setStyleSheet(styleSheet);

	Application.setStyle("fusion");


    MainWindow mainWin("");

    mainWin.show();


    Application.exec();



	Application.exit();


	return 0;//exitCode;
}

time_t time_when_compiled()
{
	using namespace std;
	// from: http://stackoverflow.com/questions/1765014/convert-string-from-date-into-a-time-t 


	string datestr = __DATE__;
	string timestr = __TIME__;


	istringstream iss_date(datestr);
	string str_month;
	int day;
	int year;
	iss_date >> str_month >> day >> year;


	int month;
	if (str_month == "Jan") month = 1;
	else if (str_month == "Feb") month = 2;
	else if (str_month == "Mar") month = 3;
	else if (str_month == "Apr") month = 4;
	else if (str_month == "May") month = 5;
	else if (str_month == "Jun") month = 6;
	else if (str_month == "Jul") month = 7;
	else if (str_month == "Aug") month = 8;
	else if (str_month == "Sep") month = 9;
	else if (str_month == "Oct") month = 10;
	else if (str_month == "Nov") month = 11;
	else if (str_month == "Dec") month = 12;
	else exit(-1);


	for (string::size_type pos = timestr.find(':'); pos != string::npos; pos = timestr.find(':', pos))
	{
		timestr[pos] = ' ';
	}


	istringstream iss_time(timestr);
	int hour, min, sec;
	iss_time >> hour >> min >> sec;


	tm t = { 0 };
	t.tm_mon = month - 1;
	t.tm_mday = day;
	t.tm_year = year - 1900;
	t.tm_hour = hour;
	t.tm_min = min;
	t.tm_sec = sec;


	return mktime(&t);
}

