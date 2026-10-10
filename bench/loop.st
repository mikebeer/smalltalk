| s t |
t := Time millisecondClock.
s := 0.
1 to: 1000000 do: [:i | s := s + (i \\ 7)].
s printNl.
(Time millisecondClock - t) displayNl.
