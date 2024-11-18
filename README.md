# datelist
This is a function that prints future dates/times on a regular schedule specfied by the user. 10 occurences are printed unless specifed by user for a different count. This was programmed for a Lunix System specifically.

### Build with
gcc -o datelist datelist.c

### Usage 
datelist \[-c \<count\>\] \<schedule\>

The user must specify the schedule at which the date must be printed. No time unit is allowed to repeat. There can be at most one occurrence of each unit. The number of occurences can be specified by the user as well using the flag of -c.

\<schedule\> \= \<num\> \<time-unit\> \[\<num\> \<time-unit\> ... \]

\<num\> \= \[1-9\]\[0-9\]...

\<time-unit\> \= year\[s\] month\[s\] week\[s\] day\[s\] hour\[s\] minute\[s\]

\[-c \<count\>\] \= count \> 0

### Features
-No time unit is allowed to repeat

-Count must be greater than 0

-time units can be listed in any order

### Defects/Shortcomings
-None that I am aware of<br><br>
This was last tested on 11/18/24 and was working as intended.

Thank you to Professor Weiss for the assigment!
