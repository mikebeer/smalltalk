| f d |
f := FileStream write: '/tmp/st_test_out.txt'.
f nextPutAll: 'line one'; nl; nextPutAll: 'line two'; nl.
f close.
f := FileStream append: '/tmp/st_test_out.txt'.
f nextPutAll: 'line three'; nl. f close.
f := FileStream read: '/tmp/st_test_out.txt'.
[f atEnd] whileFalse: [f nextLine displayNl].
(FileStream exists: '/tmp/st_test_out.txt') printNl.
(FileStream exists: '/tmp/nope') printNl.
(FileStream contentsOf: '/tmp/st_test_out.txt') size printNl.
FileStream read: '/tmp/nope'.
(Date year: 2026 month: 10 day: 10) printNl.
d := Date year: 2026 month: 10 day: 10.
d dayName printNl.
(d addDays: 30) printNl.
(d addDays: -283) printNl.
((Date year: 2028 month: 3 day: 1) subtractDate: (Date year: 2028 month: 2 day: 1)) printNl.
(Date year: 2000 month: 1 day: 1) dayName printNl.
(Date fromDays: 0) printNl.
d isLeapYear printNl.
(Date daysInMonth: 2 year: 2028) printNl.
d monthName printNl.
((Time hours: 23 minutes: 59 seconds: 30) addSeconds: 45) printNl.
((Time hours: 10 minutes: 0 seconds: 0) subtractTime: (Time hours: 9 minutes: 30 seconds: 0)) printNl.
