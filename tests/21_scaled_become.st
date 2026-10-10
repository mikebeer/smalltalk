"ScaledDecimal literals and arithmetic, become:"
1.5s2 printNl. 3s2 printNl. 0.1s printNl. 1.25s printNl.
(1.5s2 + 1.25s3) printNl. (1.5s2 * 2) printNl. (3 + 1.5s2) printNl. (1.5s2 + (1/3)) printNl.
(1/3) printNl. ((1/3) asScaledDecimal: 4) printNl. (1.5s2 / 3) printNl.
(1.5s2 < 2) printNl. (1.5s2 = 1.50s5) printNl. (1.5s2 + 1.0) printNl. 1.5s2 class printNl.
(0.1s1 + 0.2s1 = 0.3s1) printNl. 12.345s2 printNl. 1.5s2 negated printNl. 1.5s2 rounded printNl.
| a b c |
a := OrderedCollection with: 1 with: 2.
b := 'hello'.
c := Array with: a with: b.
a become: b.
c printNl.
a printNl. b printNl.
a class printNl.
