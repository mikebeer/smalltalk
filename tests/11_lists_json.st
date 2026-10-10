| l w d j |
l := LinkedList new. l add: 1; add: 2; addFirst: 0.
l printNl. l size printNl.
l removeFirst printNl. l removeLast printNl. l printNl.
l add: 5; add: 6.
(l collect: [:x | x * 2]) printNl.
(l inject: 0 into: [:a :b | a + b]) printNl.
l reverse printNl.
(l remove: 5 ifAbsent: [nil]) printNl.
l printNl.
(l includes: 6) printNl.
l removeFirst. l removeFirst.
l removeFirst.
('h*o' match: 'hello') printNl.
('h#llo' match: 'hello') printNl.
('*ell*' match: 'hello') printNl.
('h*x' match: 'hello') printNl.
('' match: '') printNl.
w := Stopwatch new. w start. w stop. w isRunning printNl.
(Stopwatch time: [100 timesRepeat: [3 + 4]]) isInteger printNl.
d := Dictionary new. d at: 'name' put: 'Ann "A"'; at: 'age' put: 30; at: 'tags' put: #('a' 'b'); at: 'ok' put: true; at: 'none' put: nil; at: 'pi' put: 3.5.
d asJson displayNl.
#(1 2 $a) asJson displayNl.
j := '{"a": [1, 2.5, "x\ny", true, null], "b": {"c": -3}}' fromJson.
(j at: 'a') printNl.
((j at: 'b') at: 'c') printNl.
j asJson displayNl.
'[]' fromJson printNl.
'{}' fromJson printNl.
'  42 ' fromJson printNl.
'"hi"' fromJson printNl.
'tru' fromJson.
