('kitten' levenshtein: 'sitting') printNl.
(27 nthRoot: 3) printNl.
(#(1 3 5 7) binarySearch: 5) printNl.
(String streamContents: [:s | s nextPutAll: 'ab'; nextPutAll: 'cd']) printNl.
((3 @ 4) dotProduct: (1 @ 2)) printNl.
(3 @ 4) r printNl.
(Date newDay: 15 month: 3 year: 2024) printNl.
(Date daysInYear: 2024) printNl.
(Date year: 2024 month: 3 day: 15) quarter printNl.
(Time hour: 13 minute: 5 second: 9) hour12 printNl.
(Duration seconds: 7200) asHours printNl.
#(1 2 3) isInterval printNl.
(1 to: 3) isInterval printNl.
nil isUndefined printNl.
$a to: $c.
($a to: $c) printNl.
