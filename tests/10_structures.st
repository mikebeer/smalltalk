(3.14159 printShowingDecimalPlaces: 2) printNl.
(2 printShowingDecimalPlaces: 2) printNl.
(-0.5 printShowingDecimalPlaces: 0) printNl.
(0.05 printShowingDecimalPlaces: 3) printNl.
(1/3 printShowingDecimalPlaces: 4) printNl.
1234567 asStringWithCommas printNl.
-1234 asStringWithCommas printNl.
999 asStringWithCommas printNl.
(7 raisedTo: 1/2) printNl.
(2 raisedTo: 0.5) printNl.
(3 + 4 i) printNl.
((3 + 4 i) * (1 - 2 i)) printNl.
((3 + 4 i) / (1 + 1 i)) printNl.
(3 + 4 i) abs printNl.
(3 + 4 i) conjugate printNl.
((3 + 4 i) = (3 + 4 i)) printNl.
(5 - (2 i)) printNl.
| s q m |
s := Stack new. s push: 1; push: 2; push: 3.
s pop printNl. s top printNl. s size printNl. s printNl.
q := Queue new. q enqueue: 1; enqueue: 2.
q dequeue printNl. q front printNl. q printNl.
s pop. s pop. s isEmpty printNl.
s pop.
m := Matrix fromRows: #(#(1 2) #(3 4)).
m printNl.
(m * m) printNl.
m transposed printNl.
m determinant printNl.
(m * 2) printNl.
(m + (Matrix identity: 2)) printNl.
(Matrix fromRows: #(#(2 0 1) #(1 3 2) #(1 1 4))) determinant printNl.
m trace printNl.
(m row: 2) printNl.
(m column: 1) printNl.
(m = m copy) printNl.
