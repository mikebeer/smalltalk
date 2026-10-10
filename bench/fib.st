Object subclass: B [ fib: n [ n < 2 ifTrue: [^n]. ^(self fib: n - 1) + (self fib: n - 2) ] ]
| t |
t := Time millisecondClock.
(B new fib: 25) printNl.
(Time millisecondClock - t) displayNl.
