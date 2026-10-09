"Fibonacci and factorial, with large integers"
Object subclass: Maths [
    Maths class >> fib: n [
        n < 2 ifTrue: [^n].
        ^(self fib: n - 1) + (self fib: n - 2)
    ]
    Maths class >> fibIter: n [
        | a b t |
        a := 0. b := 1.
        n timesRepeat: [t := a + b. a := b. b := t].
        ^a
    ]
]
(1 to: 15) do: [:i | Transcript show: (Maths fib: i) printString; show: ' '].
Transcript cr.
(Maths fibIter: 100) printNl.
(Maths fibIter: 300) printNl.
30 factorial printNl.
(100 factorial / 98 factorial) printNl.
(2 raisedTo: 200) printNl.
