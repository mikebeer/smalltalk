('a:b:c' copyUpTo: $:) printNl.
('a:b:c' copyAfter: $:) printNl.
('a:b:c' copyUpToLast: $:) printNl.
('a:b:c' copyAfterLast: $:) printNl.
'hello' capitalized printNl.
('7' padLeft: $0 to: 3) printNl.
('ab' padRightTo: 5) printNl.
('{1} is {2}' format: #('x' 42)) printNl.
('{a}-{b}' format: (Dictionary new at: #a put: 1; at: #b put: 2; yourself)) printNl.
(#(1 2 3 4 5) take: 2) printNl.
(#(1 2 3 4 5) drop: 2) printNl.
(#(1 2 3 4 5) drop: 9) printNl.
#(1 2 3) middle printNl.
#(1 2 3 4) fourth printNl.
(#(5 6) withIndexCollect: [:e :i | e * i]) printNl.
#(1 3 5 2) isSorted printNl.
#(1 2 3) isSorted printNl.
(#(1 2 3) zip: #(4 5 6)) printNl.
#(1 2 3 4) cumulative printNl.
#(1 #(2 #(3)) 4) flatten printNl.
#(#(1 2) #(3 4) #(5 6)) transpose printNl.
(#(1 2) cartesianProduct: #(3 4)) printNl.
(#(1 2 3 4) combinations: 2) printNl.
#(1 2 3) permutations printNl.
(#(1 2 3 4 5) partition: [:x | x even]) printNl.
((#(1 2 3 4 5) groupBy: [:x | x even]) at: true) printNl.
(#(1 2 3) flatCollect: [:x | Array with: x with: x]) printNl.
(#(1 2 3) detectSum: [:x | x * x]) printNl.
(#(1 2 3) detect: [:x | x > 1] ifFound: [:x | x * 10]) printNl.
(#(1 2) isSubsetOf: #(1 2 3)) printNl.
#(1 2 3 4) product printNl.
#(1 5 9) range printNl.
#(2 4 4 4 5 5 7 9) variance printNl.
#(2 4 4 4 5 5 7 9) stdev printNl.
#(1 2 2 3) mode printNl.
#(3 1 2) median printNl.
#(4 1 3 2) median printNl.
(Array with: 1->2 with: 3->4) asDictionary size printNl.
#(1 2 3 4) shuffled size printNl.
#(1 2 3) atRandom isInteger printNl.
(#(1 2 3) lastIndexOf: 3) printNl.
(OrderedCollection withAll: #(1 2 3 4)) removeFirst: 2.
((OrderedCollection withAll: #(1 2 3 4)) removeLast: 2) printNl.
| a b | a := Set withAll: #(1 2 3). b := Set withAll: #(2 3 4).
(a union: b) size printNl.
(a & b) size printNl.
(a - b) printNl.
(a symmetricDifference: b) size printNl.
(Dictionary new at: #b put: 1; at: #a put: 2; yourself) keysSorted printNl.
(Dictionary new at: 1 put: 2; yourself) invert printNl.
10 fibonacci printNl.
(5 binomial: 2) printNl.
1000 floorLog: 10.
(1000 floorLog: 10) printNl.
1994 romanNumeral printNl.
28 isPerfect printNl.
12345 digits printNl.
12345 sumOfDigits printNl.
49 isSquare printNl.
64 isPowerOfTwo printNl.
12 highBit printNl.
12 lowBit printNl.
5 bitInvert printNl.
180 degreesToRadians printNl.
(15 min: 1 max: 10) printNl.
(1 to: 3 collect: [:x | x * x]) printNl.
(ReadStream on: 'a,b,c') skipTo: $,; upToEnd.
((ReadStream on: 'a,b,c') skipTo: $,; upToEnd) printNl.
(ReadStream on: 'x
y') lines printNl.
(WriteStream on: String new) crtab; contents.
(3 in: [:x | x + 1]) printNl.
3 asString printNl.
[:x | x] isBlock printNl.
[1/0] ifError: [:m | m displayNl].
