| a oc d s b st |
a := Array new: 3.
a at: 1 put: 10; at: 2 put: 20; at: 3 put: 30.
a printNl.
(a copyWith: 40) printNl.
(a , #(1 2)) printNl.
(a indexOf: 20) printNl.
a isEmpty printNl. a notEmpty printNl. a size printNl.
(a includes: 20) printNl.
(a detect: [:x | x > 15]) printNl.
(a detect: [:x | x > 100] ifNone: ['none']) printNl.
(a anySatisfy: [:x | x > 25]) printNl.
a first printNl. a last printNl.
(a copyFrom: 2 to: 3) printNl.
a reverse printNl.
#(5 3 9 1) asSortedCollection asArray printNl.
(#(5 3 9 1) asSortedCollection: [:x :y | x >= y]) printNl.
(#('pear' 'apple' 'fig') asSortedCollection: [:x :y | x size <= y size]) asOrderedCollection printNl.
#(5 3 9 1) sort printNl.
(#(5 3 9 1) sort: [:x :y | x > y]) printNl.
oc := OrderedCollection new.
oc add: 3; add: 1; addFirst: 7; add: 9.
oc printNl.
oc removeFirst printNl. oc removeLast printNl. oc printNl.
(oc add: 5 beforeIndex: 1) printNl. oc printNl.
(oc remove: 99 ifAbsent: ['absent']) printNl.
oc remove: 3. oc printNl.
oc removeFirst. oc removeFirst. oc isEmpty printNl.
oc removeFirst.
'next' displayNl.
oc := OrderedCollection new. 1 to: 100 do: [:i | oc addFirst: i]. oc size printNl. oc first printNl. oc last printNl.
(oc at: 50) printNl. oc at: 500.
d := Dictionary new.
d at: 'one' put: 1; at: 'two' put: 2; at: 'three' put: 3.
d printNl.
(d at: 'two') printNl.
(d removeKey: 'two') printNl.
d size printNl.
(d includesKey: 'one') printNl. (d includesKey: 'two') printNl.
(d keyAtValue: 3) printNl.
d keysAndValuesDo: [:k :v | Transcript showCr: k, '=', v printString].
(d collect: [:v | v * 10]) printNl.
(d select: [:v | v > 1]) printNl.
(d associations) printNl.
d keys asSortedCollection printNl.
(d inject: 0 into: [:acc :v | acc + v]) printNl.
d at: #missing.
s := Set new. s add: 1; add: 2; add: 1; add: 'a'; add: 'a'.
s size printNl. (s includes: 'a') printNl. s remove: 1. s size printNl.
b := Bag new. b add: 'x'; add: 'y'; add: 'x'.
(b occurrencesOf: 'x') printNl. b size printNl.
st := Set withAll: #(1 2 3). (st collect: [:x | x \\ 2]) printNl.
(1 to: 5) asArray printNl. (5 to: 1 by: -2) asArray printNl.
((1 to: 10) select: [:x | x odd]) printNl.
((1 to: 5) collect: [:x | x * x]) printNl.
((1 to: 5) inject: 0 into: [:x :y | x + y]) printNl.
(1 to: 5) reverse printNl.
#(1 2 3) with: #(4 5 6) do: [:x :y | Transcript show: (x * y) printString; show: ' ']. Transcript cr.
(#(1 2 3) with: #(4 5 6) collect: [:x :y | x + y]) printNl.
#(1 2 3) doWithIndex: [:e :i | Transcript show: i printString, ':', e printString, ' ']. Transcript cr.
#(1 2 3) keysAndValuesDo: [:i :e | Transcript show: i printString, ':', e printString, ' ']. Transcript cr.
(Array new: 2) printNl.
(Array new: 2 withAll: 7) printNl.
(Array with: 1 with: 2 with: 3) printNl.
(OrderedCollection withAll: #(3 4)) printNl.
#(1 2 3) max printNl.
(#(1 2 3 4) copyWithout: 3) printNl.
#($a $b) asString printNl.
