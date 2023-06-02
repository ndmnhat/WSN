struct date_struct {
    unsigned int year;
    unsigned int month;
    unsigned int day;
    unsigned int wday;
    char ok;
};

struct time_struct {
    unsigned int hour;
    unsigned int minute;
    unsigned int second;
    unsigned int fraction;
    int offset; // UTC offset in minutes
    char ok;
};

struct date_time_struct {
    date_struct date;
    time_struct time;
    char ok;
} ;

void _utcnow(date_time_struct *now);
void _format_date_time(date_time_struct *dt, char* datetime_string);