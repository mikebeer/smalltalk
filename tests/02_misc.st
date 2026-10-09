Object subclass: Counter [
    | n |
    Total := 0.
    Counter class [ | made | ]
    Counter class >> new [ made := (made ifNil: [0]) + 1. ^super new init ]
    Counter class >> made [ ^made ]
    init [ n := 0 ]
    incr [ n := n + 1. Total := Total + 1. ^self ]
    n [ ^n ]
    Counter class >> total [ ^Total ]
    find: x in: coll [ coll do: [:e | e = x ifTrue: [^'found']]. ^'missing' ]
    makeAdder: k [ ^[:x | x + k] ]
]
| c add5 s ws rs |
c := Counter new.
c incr; incr; incr.
c n printNl.
Counter new incr.
Counter total printNl.
Counter made printNl.
(c find: 3 in: #(1 2 3)) displayNl.
(c find: 9 in: #(1 2 3)) displayNl.
add5 := c makeAdder: 5.
(add5 value: 10) printNl.
Counter printNl. Counter class printNl. Counter class class printNl.
Counter superclass printNl. Counter name printNl. Counter category printNl.
c class printNl. c isString printNl. c respondsTo: #incr.
(c respondsTo: #incr) printNl. (c respondsTo: #zork) printNl.
(c isKindOf: Object) printNl.
3 class printNl. (2 raisedTo: 70) class printNl. 3.5 class printNl. (1/2) class printNl.
nil class printNl. true class printNl. 'a' class printNl. #a class printNl. $a class printNl. [] class printNl.
ws := WriteStream on: String new.
ws nextPutAll: 'Hello'; nextPut: $,; print: 42; nl; tab; nextPutAll: 'x'.
ws contents displayNl.
rs := ReadStream on: #(1 2 3 4 5).
[rs atEnd] whileFalse: [rs next printNl].
(ReadStream on: 'hello world') upTo: $ .
((ReadStream on: 'hello world') upTo: $ ) displayNl.
'a b  c' subStrings printNl.
('a,b,c' subStrings: ',') printNl.
('hello' indexOf: $l) printNl.
('hello' copyFrom: 2 to: 4) displayNl.
('hello' copyReplaceAll: 'l' with: 'L') displayNl.
'hello' asSymbol printNl. #foo:bar: numArgs printNl.
'42' asNumber printNl. '3.5' asNumber printNl. 'abc' asNumber printNl.
(3 max: 9) printNl. 3 even printNl. 7 isPrime printNl. (12 gcd: 18) printNl.
5 timesRepeat: [Transcript show: '*']. Transcript cr.
1 to: 3 do: [:i | Transcript show: i printString; show: ' ']. Transcript cr.
3 to: 1 by: -1 do: [:i | Transcript show: i printString; show: ' ']. Transcript cr.
#(1 2 3) do: [:e | Transcript show: e printString] separatedBy: [Transcript show: ', ']. Transcript cr.
#(1 2 3) reverse printNl. #(1 2 3) reverseDo: [:e | e printNl].
(#(1 2 3) collect: [:e | e * e]) printNl.
#(1 $a 'str' #sym (1 2) #(3 4)) printNl.
{1+1. 'two'. #three} printNl.
(1 = 1) printNl. (1 = 2) printNl. ('abc' = 'abc') printNl. ('abc' == 'abc') printNl. (#a == #a) printNl.
(1 -> 2) printNl.
(3 @ 4) printNl. ((3 @ 4) + (1 @ 1)) printNl.
1e10 printNl. 1.5e-5 printNl. 100 factorial printString size printNl.
-5 abs printNl. 5 negated printNl. 3.7 floor printNl. 3.2 ceiling printNl. 3.5 rounded printNl. 3.7 truncated printNl.
(1 to: 0) printNl. #() printNl. '' printNl.
Smalltalk at: #Gx put: 42. (Smalltalk at: #Gx) printNl. Gx printNl.
(Smalltalk includesKey: #Gx) printNl.
Object subclass: #OldStyle instanceVariableNames: 'a b' classVariableNames: 'K' package: 'x'.
OldStyle extend [ a [ ^a ] a: x [ a := x ] ].
(OldStyle new a: 5; yourself) a printNl.
