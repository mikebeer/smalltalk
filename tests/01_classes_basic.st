Object subclass: Animal [
    | name sound |
    Animal class >> named: aName [ ^self new setName: aName ]
    setName: aName [ name := aName ]
    name [ ^name ]
    speak [ ^self subclassResponsibility ]
    describe [ ^name, ' says ', self speak ]
    printOn: aStream [ aStream nextPutAll: 'Animal(', name, ')' ]
]
Animal subclass: Dog [
    speak [ ^'Woof' ]
]
Animal subclass: Cat [
    speak [ ^'Meow' ]
    describe [ ^super describe, '!' ]
]
(Dog named: 'Rex') describe displayNl.
(Cat named: 'Tom') describe displayNl.
(Dog named: 'Rex') printNl.
(Animal named: 'x') speak.
'after' displayNl.
| a |
a := OrderedCollection new.
1 to: 5 do: [:i | a add: i * i].
a printNl.
(a collect: [:x | x + 1]) printNl.
(a select: [:x | x even]) printNl.
(a inject: 0 into: [:s :x | s + x]) printNl.
#(3 1 2) asSortedCollection printNl.
(#(3 1 2) asSortedCollection: [:x :y | x > y]) asArray printNl.
(1 to: 5) printNl.
(1 to: 10 by: 2) asOrderedCollection printNl.
'hello world' asUppercase displayNl.
'hello' reverse displayNl.
('hello' at: 1) printNl.
#sym printNl.
$a printNl.
3.14159 printNl.
(1/3) + (2/3) printNl.
((1/3) + (2/3)) printNl.
(2 raisedTo: 100) printNl.
1000000000 * 1000000000 * 1000000000.
(1000000000 * 1000000000 * 1000000000) printNl.
10 sqrt printNl.
(17 \\ 5) printNl. (17 // 5) printNl. (-17 \\ 5) printNl. (-17 // 5) printNl.
(Set withAll: #(1 2 2 3)) printNl.
(Bag withAll: #(1 2 2 3)) size printNl.
| d |
d := Dictionary new.
d at: #a put: 1; at: #b put: 2.
d printNl.
(d at: #a) printNl.
(d at: #z ifAbsent: [0]) printNl.
d keys printNl.
d values printNl.
d at: #zz.
[:x :y | x + y] valueWithArguments: #(1 2).
[^3] value.
'x' displayNl.
| r |
r := [:x | x > 2 ifTrue: ['big'] ifFalse: ['small']] value: 3.
r displayNl.
[Error signal: 'boom'] on: Error do: [:e | e messageText displayNl].
[Error signal: 'boom'. 'not here' displayNl] on: Error do: [:e | e return: 5].
([Error signal: 'boom'. 7] on: Error do: [:e | e return: 5]) printNl.
([Warning signal: 'w'. 7] on: Warning do: [:e | e resume: 5]) printNl.
([(Warning signal: 'w') printNl. 7] on: Warning do: [:e | e resume: 5]) printNl.
[#(1 2 3) at: 10] on: Error do: [:e | e messageText displayNl].
#(1 2 3) at: 10.
[1/0] ensure: ['ensured' displayNl].
'end' displayNl.
(3 > 2) | (1 > 2) printNl.
