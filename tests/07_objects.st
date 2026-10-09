'Hello, World' asSymbol printNl. #foo printNl. #+ printNl. #with:with: printNl. SmallInteger maxVal printNl.
Integer extend [ double [ ^self * 2 ] ].
5 double printNl.
Object subclass: Stack [
  | items |
  init [ items := OrderedCollection new ]
  Stack class >> new [ ^super new init ]
  push: x [ ^items addLast: x ]
  pop [ items isEmpty ifTrue: [^self error: 'stack empty']. ^items removeLast ]
  top [ ^items last ]
  isEmpty [ ^items isEmpty ]
  size [ ^items size ]
  do: aBlock [ items reverseDo: aBlock ]
]
Stack subclass: CountingStack [
  | pushes |
  init [ super init. pushes := 0 ]
  push: x [ pushes := pushes + 1. ^super push: x ]
  pushes [ ^pushes ]
]
| s |
s := CountingStack new.
s push: 1; push: 2; push: 3.
s pop printNl. s pushes printNl. s size printNl.
s do: [:e | e printNl].
s pop. s pop. s pop.
'ok' displayNl.
Object subclass: Node [ |v next| v [ ^v ] v: x [ v := x ] next [ ^next ] next: n [ next := n ] ]
| head |
head := nil.
1 to: 3 do: [:i | head := Node new v: i; next: head; yourself].
[head notNil] whileTrue: [head v printNl. head := head next].
Object subclass: Deep [ down: n [ ^n = 0 ifTrue: [0] ifFalse: [1 + (self down: n - 1)] ] ]
(Deep new down: 2000) printNl.
'survived' displayNl.
Object subclass: Blk [ find [ #(1 2 3) do: [:x | x = 2 ifTrue: [^x]]. ^0 ]  mk [ ^[:x | ^x] ] ]
Blk new find printNl.
(Blk new mk value: 3) printNl.
'after dead return' displayNl.
Object subclass: Blk [ mk [ ^[:x | ^x] ] ]
(Blk new mk value: 3) printNl.
'after dead return' displayNl.
