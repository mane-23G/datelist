/*
Title: datelist.c
Author: Mane Galstyan
Created on: Feb 27, 2024
Description: print future dates/times on a regular schedule
Purpose: print future dates/times on a regular schedule specfied by the user for 10 occurences unless specifed by user for a different count
Usage: datelist [-c <count>] <schedule>
Build with: gcc -o datelist datelist.c
Modifications: March 4, 2024 clean up and comments

******************************************************************************
* Copyright (C) 2023 - Stewart Weiss                                         *
*                                                                            *
* This code is free software; you can use, modify, and redistribute it       *
* under the terms of the GNU General Public License as published by the      *
* Free Software Foundation; either version 3 of the License, or (at your     *
* option) any later version. This code is distributed WITHOUT ANY WARRANTY;  *
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
* PARTICULAR PURPOSE. See the file COPYING.gplv3 for details.                *
*****************************************************************************/

#define _GNU_SOURCE
#include <langinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <locale.h>

/* Define various constants and types used throughout the examples.   */
#define  STRING_MAX  1024

/* Create a BOOL type */
#ifdef FALSE
    #undef FALSE
#endif
#ifdef TRUE
    #undef TRUE
#endif

#ifdef BOOL
    #undef BOOL
#endif
typedef enum{FALSE, TRUE} BOOL;


#define MAXLEN  STRING_MAX        /* Maximum size of message string         */

/* These are used by locale-related programs */
#define BAD_FORMAT_ERROR    -1    /* Error in format string                 */
#define TIME_ADJUST_ERROR   -2    /* Error to return if parsing problem     */
#define LOCALE_ERROR        -3    /* Non-specific error from setlocale()    */

/* flags to pass to functions */
#define NO_TRAILING           1     /* forbid trailing characters     */
#define NON_NEG_ONLY          2     /* forbid negative numbers        */
#define ONLY_DIGITS           4     /* forbid strings with no digits  */
#define PURE   NO_TRAILING | ONLY_DIGITS

/* return codes */
#define VALID_NUMBER          0     /* successful processing                 */
#define FATAL_ERROR          -1     /* ERANGE or EINVAL returned by strtol() */
#define TRAILING_CHARS_FOUND -2     /* characters found after number         */
#define OUT_OF_RANGE         -3     /* int requested but out of int range    */
#define NO_DIGITS_FOUND      -4     /* no digits in string                   */
#define NEG_NUM_FOUND        -5     /* negative number found but not allowed */

/* error_message()
   This prints an error message associated with errnum  on standard error
   if errnum > 0. If errnum <= 0, it prints the msg passed to it.
   It does not terminate the calling program.
   This is used when there is a way  to recover from the error.               */
void error_mssge(int errornum, const char * msg) {
    errornum > 0 ? fprintf(stderr, "%s\n", msg) : fprintf(stderr, "%s\n", msg);
}

/* fatal_error()
   This prints an error message associated with errnum  on standard error
   before terminating   the calling program, if errnum > 0.
   If errnum <= 0, it prints the msg passed to it.
   fatal_error() should be called for a nonrecoverable error.                 */
void fatal_error(int errornum, const char * msg) {
    error_mssge(errornum, msg);
    exit(EXIT_FAILURE);
}

/* usage_error()
   This prints a usage error message on standard output, advising the
   user of the correct way to call the program, and terminates the program.   */
void usage_error(const char * msg) {
    fprintf(stderr, "usage: %s\n", msg);
    exit(EXIT_FAILURE);
}

/*
On successful processing, returns SUCCESS and stores the resulting value
in value, otherwise returns FAILURE.
On return, msg contains a string with a suitable message for the caller
to pass to an error-handling function.
flags is used to decide whether trailing characters or negatives are allowed.
*/
int  get_long(char *arg, int flags, long *value, char *msg ) {
    char *endptr;
    long val;
    errno = 0;    /* To distinguish success/failure after call */
    val = strtol(arg, &endptr, 0);

    /* Check for various possible errors */
    if (errno == ERANGE) {
        if ( msg != NULL )
            sprintf(msg, "%s\n", strerror(errno));
        return FATAL_ERROR;
    } else if ( errno == EINVAL && val != 0 ) {
          /* bad base chars -- shouldn't happen */
          if ( msg != NULL )
              sprintf(msg, "%s\n", strerror(errno));
          return FATAL_ERROR;
    }
    /* errno == 0 or errno == EINVAL && val == 0 */

    if (endptr == arg) {
        /* the first invalid char is the first char of the string */
        if ( flags & ( ONLY_DIGITS | NO_TRAILING ) ) {
            if ( msg != NULL )
                sprintf(msg, "No digits in the string\n");
            return NO_DIGITS_FOUND;
        }
        else { /* accept a zero result */
            *value = 0;
            return VALID_NUMBER;
        }
    }
    /* endptr != arg but endptr is not at end of string */

    if (*endptr != '\0' && *endptr != ' ') {
      /* there are non-number characters following the number.
         which we can call an error or not, depending. */
        if ( flags & NO_TRAILING ) {
            if ( msg != NULL )
                sprintf(msg, "Trailing characters follow the number: \"%s\"\n",
                endptr);
            return TRAILING_CHARS_FOUND;
        }
    }

    /* If we got here, strtol() successfully parsed a number, but it might be
       negative, so check flag and report if necessary */
    if ( flags & NON_NEG_ONLY )
        if ( val < 0 ) {
            if (msg != NULL )
                sprintf(msg, "number is negative\n");
            arg = endptr;
            return NEG_NUM_FOUND;
        }

    *value = val;
    return VALID_NUMBER;
}

int  get_int(char *arg, int flags, int *value, char *msg )
{
    long val;
    int res =  get_long(arg, flags, &val, msg );

    if ( VALID_NUMBER == res ) {
        if ( val > INT_MAX  || val < INT_MIN ) {
            sprintf(msg, "%ld is out of range\n", val);
            return OUT_OF_RANGE;
        }
        else {
            *value = val;
            return VALID_NUMBER;
        }
    }
    else { /* get_long failed in one way or another */
        return res;
    }
}

int parse_time_adjustment( char* datestring, struct tm* datetm, char format[])
{
    char* delim   = " \t";     /* Space and tab             */
    char* token;               /* Returned token            */
    int   number;              /* To store number token     */
    char  err_msg[STRING_MAX]; /* For error messages        */
    int   flags   = ONLY_DIGITS | NO_TRAILING;
    int   res;                 /* Return value of get_int() */
    BOOL year = FALSE, month = FALSE, week = FALSE, day = FALSE, hour = FALSE, minute = FALSE; /*keeps track of duplicate time units*/
    /* Initialize strtok() and set token to first word in string.           */
    token = strtok(datestring, delim);

    /* Processing ends if strtok() returns a NULL pointer. */
    while ( token != NULL ) {
        /* Expecting an integer */
        res =  get_int(token, flags, &number, err_msg );
        if ( VALID_NUMBER != res )
            fatal_error(res, err_msg);
        
        /* num is quantity of time-adjustment unit to be found next
         get next token in time adjustment, should be a time unit.        */
        token = strtok(NULL, delim);
        if ( token == NULL ) {
            /* end of string encountered without the time unit*/
            fatal_error(TIME_ADJUST_ERROR, "missing a time unit\n");
        }
        
        /* Add num units to member of struct tm. */
        if ( NULL != strstr(token, "year")) {
            if(year == TRUE) return -1;
            datetm->tm_year += number;
            year = TRUE;
        }
        else if ( NULL != strstr(token, "month")) {
            if(month == TRUE) return -1;
            datetm->tm_mon += number;
            month = TRUE;
        }
        else if ( NULL != strstr(token, "week")) {
            if(week == TRUE) return -1;
            datetm->tm_mday += 7*number;
            week = TRUE;
        }
        else if ( NULL != strstr(token, "day"))  {
            if(day == TRUE) return -1;
            datetm->tm_mday += number;
            day = TRUE;
        }
        else if ( NULL != strstr(token, "hour")) {
            if(hour == TRUE) return -1;
            datetm->tm_hour += number;
            hour = TRUE;
        }
        else if ( NULL != strstr(token, "minute")){
            if(minute == TRUE) return -1;
            datetm->tm_min += number;
            minute = TRUE;
        }
        else
            /* Time_unit did not match any valid time time_unit.            */
            fatal_error(TIME_ADJUST_ERROR, "Found invalid time time_unit in amount to adjust the time\n");
        token = strtok(NULL, delim);
    }
    
    /*modifies format if time unit less than day has been specfied by user*/
    if((minute == TRUE || hour == TRUE))
        strcpy(format, "%x %X");
    return 0;
}

int update_time( struct tm* datetm, struct tm* date_to_add )
{
    datetm->tm_year += date_to_add->tm_year;
    datetm->tm_mon  += date_to_add->tm_mon;
    datetm->tm_mday += date_to_add->tm_mday;
    datetm->tm_hour += date_to_add->tm_hour;
    datetm->tm_min  += date_to_add->tm_min;

    errno = 0;
    mktime(datetm);
    if ( errno != 0 )
        fatal_error(errno, NULL);

    return 0;
}
 

int main(int argc, char *argv[]) {
    
    char formatted_date[MAXLEN];     /* String storing formatted date      */
    time_t current_time;             /*Timeval in seconds since Epoch      */
    struct tm *bdtime;               /* Broken-down time                   */
    struct tm time_adjustment= {0};  /* Broken-down time for adjustment    */
    char usage_msg[512];             /* Usage message                      */
    char ch;                         /* For option handling                */
    char options[] = "c:h";         /* getopt string                      */
    BOOL c_option = FALSE;           /* Flag to indicate -d found          */
    char *c_arg;                     /* Dynamic string for -d argument     */
    int c_arg_length;                /* Length of -d argument string       */
    long count = 10;
    int i = 0;
    char format[MAXLEN] = "%x"; /* default format subject to change depending on arguments*/
    opterr = 0;  /* Turn off error messages by getopt().                    */
    
    if(argc < 2) {
        sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
        usage_error(usage_msg);
    }
    
    char*  mylocale;
    if ( (mylocale = setlocale(LC_TIME, "") ) == NULL )
        fatal_error(LOCALE_ERROR, "setlocale() could not set the given locale");
    
    while(TRUE) {
        /* Call getopt, passing argc and argv and the options string.       */
        ch = getopt(argc, argv, options);
        if ( -1 == ch ) /* It returns -1 when it finds no more options.     */
            break;

        switch (ch) {
        case 'c':   /* Has required argument. */
                c_option = TRUE;
                c_arg_length = strlen(optarg);
                c_arg = malloc((c_arg_length+1) * sizeof(char));
                if (NULL == c_arg )
                fatal_error(EXIT_FAILURE, "calloc could not allocate memory\n");
                strcpy(c_arg, optarg);
            break;
        case 'h':   /* Help message */
            sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
            usage_error(usage_msg);
        case '?' : /* either option is present without argument or option is incorrect since '?' doesnt tell you what the error is exactly*/
                if(optopt == 'c') {
                    fprintf(stderr,"Missing argument for option\n");
                    sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
                    usage_error(usage_msg);
                    break;
                }
                else {
                    fprintf(stderr,"Found invalid option\n");
                    sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
                    usage_error(usage_msg);
                    break;
                }
        case ':' :
            fprintf(stderr,"Found option with missing argument\n");
            sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
            usage_error(usage_msg);
            break;
        }
    }
    
    
    if(c_option) {
        errno = 0;
        count = strtol(c_arg, NULL, 10);
        if(errno == ERANGE || errno == EINVAL || count <= 0)
            fatal_error(ERANGE, "Found invalid option argument");
        free(c_arg);
    }
    
    if (optind >= argc) {
        fprintf(stderr,"No argument found\n");
        sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
        usage_error(usage_msg);
    }
    
    /* Get the current time.           */
    current_time = time(NULL);

    /* Convert the current time into broken-down time. */
    bdtime = localtime(&current_time);

    /* The only possible error is EOVERFLOW, and localtime returns NULL
       if the error occurred. */
    if(bdtime == NULL)
        fatal_error(EOVERFLOW, "localtime");

    /* if arguments are not in qoutes, the arguments are */
    char combined_args[MAXLEN] = ""; /* for concatenated strings*/
    int len = 0, arg_len,space_len; /* keep track of the lenght of the string*/
    for (int index = optind; index < argc; index++) {
	arg_len = strlen(argv[index]);
	space_len = len > 0 ? 1 : 0;
	if(len + arg_len + space_len >= MAXLEN) {
	   fatal_error(BAD_FORMAT_ERROR, "Argument is too long.\n");

	}
        if(space_len) 
	   strcat(combined_args, " ");

       strcat(combined_args, argv[index]);
       len += arg_len;
     }
    
    /* do the time adjustment the user wanted if -1 is returned the user put the same unit twice*/
    if(parse_time_adjustment(combined_args, &time_adjustment, format) == -1) {
        fprintf(stderr,"Duplicate time unit found.\n");
        sprintf(usage_msg, "%s [-c <count>] <schedule>", argv[0]);
        usage_error(usage_msg);
    }
    
    /* print out the list of times with adjuctment for count times*/
    while(i < count) {
        update_time(bdtime, &time_adjustment);
        
        if (0 == strftime(formatted_date, sizeof(formatted_date),format, bdtime) )
        /* strftime does not set errno. If return is 0, it is an error
         because we expect a non-zero number of bytes in the output
         string. */
            fatal_error(BAD_FORMAT_ERROR, "Conversion to a date-time string failed or produced an empty string\n");
        
        /* Print the formatted date to standard output.  */
        printf("%s\n", formatted_date);
        i++;
    }

    return 0;
}
 
