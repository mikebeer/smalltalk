Object subclass: C [
  | n |
  init [ n := 0 ]
  inc [ n := n + 1 ]
  n [ ^n ]
  run: k [ k timesRepeat: [self inc]. ^self n ]
]
| t c |
t := Time millisecondClock.
c := C new init; yourself.
(c run: 300000) printNl.
(Time millisecondClock - t) displayNl.
