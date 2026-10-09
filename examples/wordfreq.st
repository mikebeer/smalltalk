"Count word frequencies read from standard input"
| counts line |
counts := Dictionary new.
[Smalltalk stdinAtEnd] whileFalse: [
    line := Smalltalk readLine.
    line isNil ifFalse: [
        line asLowercase subStrings do: [:w |
            counts at: w put: (counts at: w ifAbsent: [0]) + 1]]].
(counts associations asSortedCollection: [:a :b |
    a value > b value or: [a value = b value and: [a key <= b key]]]) do: [:a |
    Transcript showCr: a key, ' ', a value printString].
