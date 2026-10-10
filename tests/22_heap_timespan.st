"Heap, Timespan, Transcript capture/redirect, LookupTable, WeakSet"
| h t s a b |
h := Heap new. #(5 3 9 1 7 2 8) do: [:e | h add: e].
h size printNl. h first printNl.
[h isEmpty] whileFalse: [Transcript show: h removeFirst printString; space]. Transcript cr.
h := Heap sortBlock: [:x :y | x >= y]. h add: 1; add: 5; add: 3. h removeFirst printNl.
(Heap withAll: #(4 2 6)) asSortedArray printNl.
s := Transcript capture: [Transcript showCr: 'captured'. 3 printNl. Transcript print: 42].
s printNl.
Transcript showCr: 'back to stdout'.
a := WriteStream on: String new. Transcript redirectTo: a. Transcript showCr: 'hidden'. Transcript stopRedirect. a contents printNl.
t := Timespan starting: (DateTime year: 2026 month: 10 day: 10 hour: 8 minute: 0 second: 0) duration: 90 minutes.
t printNl. t end printNl.
(t includes: (DateTime year: 2026 month: 10 day: 10 hour: 9 minute: 0 second: 0)) printNl.
(t includes: (DateTime year: 2026 month: 10 day: 10 hour: 10 minute: 0 second: 0)) printNl.
LookupTable new printNl. WeakSet new class superclass printNl.
