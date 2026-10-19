"Garbage collector test (C version only; the CREXX version has no collector).
 Run in bounded memory: tests/run_tests.sh checks the maximum resident set size."
Object subclass: Churn [
    | keep |
    run: n [
        | d s oc t |
        d := Dictionary new.
        oc := OrderedCollection new.
        t := 0.
        1 to: n do: [:i |
            s := 'item ', i printString.
            d at: i \\ 100 put: s.
            d removeKey: (i + 50) \\ 100 ifAbsent: [nil].
            oc add: (Array new: 20).
            oc size > 50 ifTrue: [oc removeFirst].
            t := t + (self twice: [:x | x + s size]) ].
        keep := d.
        ^t + d size + oc size
    ]
    twice: aBlock [ ^(aBlock value: 1) + (aBlock value: 2) ]
]
| s c n sum a keepers w |
"1. repeated string concatenation: O(N) live memory"
n := 3000.
s := ''.
0 to: n - 1 do: [:i | s := s , (String with: (Character value: 65 + (i \\ 26)))].
c := 0.
1 to: s size do: [:i | (s at: i) = $E ifTrue: [c := c + 1]].
s size printNl.
c printNl.
"2. a long allocation loop: strings, arrays, blocks, floats, big integers"
sum := 0.
1 to: 4000 do: [:i |
    a := Array new: 8.
    a at: 1 put: i printString; at: 2 put: i * 1.5; at: 3 put: [:x | x + i].
    sum := sum + ((a at: 3) value: 1) + (a at: 1) size].
sum printNl.
"3. data that must survive many collections, built before and checked after"
keepers := OrderedCollection new.
1 to: 300 do: [:i | keepers add: (Array with: i printString with: 30 factorial + i with: (Set with: i with: i + 1))].
w := Churn new.
(w run: 3000) printNl.
sum := 0.
keepers doWithIndex: [:e :i |
    ((e at: 1) = i printString and: [(e at: 2) = (30 factorial + i) and: [(e at: 3) size = 2]])
        ifTrue: [sum := sum + 1]].
sum printNl.
(30 factorial) printNl.
