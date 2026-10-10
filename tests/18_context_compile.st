"thisContext, evaluate:, compile:"
Object subclass: B [
    foo [ ^self bar ]
    bar [ ^thisContext ]
    baz [ thisContext backtrace ]
    qux [ ^self baz ]
]
Object subclass: A [ ]
B new foo printNl.
B new foo sender printNl.
B new foo depth printNl.
B new foo receiver class printNl.
B new foo selector printNl.
B new qux.
(Smalltalk evaluate: '3 + 4') printNl.
(Smalltalk evaluate: '#(1 2 3) inject: 0 into: [:a :b | a + b]') printNl.
A compile: 'double: x [ ^x * 2 ]'.
(A new double: 4) printNl.
A compile: 'triple: x ^x * 3'.
(A new triple: 4) printNl.
A class compile: 'make [ ^self new ]'.
A make printNl.
A compile: 'sum: n
    | t |
    t := 0.
    1 to: n do: [:i | t := t + i].
    ^t'.
(A new sum: 10) printNl.
(A includesSelector: #double:) printNl.
Smalltalk fileIn: 'Object subclass: Gen [ answer [ ^42 ] ]'.
Gen new answer printNl.
