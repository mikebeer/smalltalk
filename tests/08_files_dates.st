| f |
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
