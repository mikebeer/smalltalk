"extras.st - additional convenience methods"

Object extend [
    displayOn: aStream [ aStream nextPutAll: self displayString ]
    in: aBlock [ ^aBlock value: self ]
    asString [ ^self displayString ]
    assert: aBoolean description: aString [ aBoolean ifFalse: [^self error: aString] ]
    deny: aBoolean [ aBoolean ifTrue: [^self error: 'Assertion failed'] ]
    isBlock [ ^false ]
    isException [ ^false ]
    isDate [ ^false ]
    isTime [ ^false ]
    isRational [ ^false ]
    isMeta [ ^false ]
    instVarNamed: aName [
        | i |
        i := self class allInstVarNames indexOf: aName asSymbol.
        i = 0 ifTrue: [^self error: 'no such instance variable'].
        ^self instVarAt: i
    ]
    instVarNamed: aName put: v [
        | i |
        i := self class allInstVarNames indexOf: aName asSymbol.
        i = 0 ifTrue: [^self error: 'no such instance variable'].
        ^self instVarAt: i put: v
    ]
]
Date extend [ isDate [ ^true ] ]
Time extend [ isTime [ ^true ] ]
BlockClosure extend [
    isBlock [ ^true ]
    cull: a [ ^self numArgs = 0 ifTrue: [self value] ifFalse: [self value: a] ]
    timeToRun [ ^Time millisecondsToRun: self ]
    millisecondsToRun [ ^Time millisecondsToRun: self ]
    ifError: handler [ ^self on: Error do: [:e | handler cull: e messageText] ]
    asString [ ^'a BlockClosure' ]
]
Integer extend [ isRational [ ^true ] ]
Fraction extend [ isRational [ ^true ] ]

Behavior extend [
    allSelectors [
        | r c |
        r := Set new. c := self.
        [c notNil] whileTrue: [r addAll: c selectors. c := c superclass].
        ^r asSortedCollection asArray
    ]
    selectorsDo: aBlock [ self selectors do: aBlock ]
    withAllSubclasses [ ^(OrderedCollection with: self) addAll: self allSubclasses; yourself ]
    lookupSelector: aSymbol [
        | c |
        c := self.
        [c notNil] whileTrue: [(c includesSelector: aSymbol) ifTrue: [^c]. c := c superclass].
        ^nil
    ]
    isVariable [ ^false ]
    printHierarchy [ self printHierarchyIndent: 0 ]
    printHierarchyIndent: n [
        n * 2 timesRepeat: [Transcript show: ' '].
        Transcript show: self name; cr.
        self subclasses do: [:c | c printHierarchyIndent: n + 1]
    ]
]

Collection extend [
    partition: aBlock [
        | a b |
        a := OrderedCollection new. b := OrderedCollection new.
        self do: [:e | (aBlock value: e) ifTrue: [a add: e] ifFalse: [b add: e]].
        ^Array with: a asArray with: b asArray
    ]
    groupBy: aBlock [
        | d |
        d := Dictionary new.
        self do: [:e | (d at: (aBlock value: e) ifAbsentPut: [OrderedCollection new]) add: e].
        ^d
    ]
    flatCollect: aBlock [
        | r |
        r := OrderedCollection new.
        self do: [:e | r addAll: (aBlock value: e)].
        ^r asArray
    ]
    detectSum: aBlock [ ^self inject: 0 into: [:a :e | a + (aBlock value: e)] ]
    detect: aBlock ifFound: fBlock [
        self do: [:e | (aBlock value: e) ifTrue: [^fBlock value: e]].
        ^nil
    ]
    isSubsetOf: other [ ^self allSatisfy: [:e | other includes: e] ]
    product [ ^self inject: 1 into: [:a :b | a * b] ]
    range [ ^self max - self min ]
    variance [
        | m |
        m := self average.
        ^(self inject: 0 into: [:a :b | a + (b - m) squared]) / self size
    ]
    stdev [ ^self variance sqrt ]
    mode [
        | d best bc |
        d := Dictionary new.
        self do: [:e | d at: e put: (d at: e ifAbsent: [0]) + 1].
        bc := 0.
        d keysAndValuesDo: [:k :v | v > bc ifTrue: [best := k. bc := v]].
        ^best
    ]
    median [
        | s n |
        s := self asSortedCollection asArray.
        n := s size.
        ^n odd ifTrue: [s at: n + 1 // 2] ifFalse: [(s at: n // 2) + (s at: n // 2 + 1) / 2]
    ]
    asDictionary [
        | d |
        d := Dictionary new.
        self do: [:a | d at: a key put: a value].
        ^d
    ]
]

SequenceableCollection extend [
    lastIndexOf: x [
        self size to: 1 by: -1 do: [:i | (self at: i) = x ifTrue: [^i]].
        ^0
    ]
    copyUpTo: x [
        | i |
        i := self indexOf: x.
        i = 0 ifTrue: [^self copy].
        ^self copyFrom: 1 to: i - 1
    ]
    copyAfter: x [
        | i |
        i := self indexOf: x.
        i = 0 ifTrue: [^self copyFrom: 1 to: 0].
        ^self copyFrom: i + 1 to: self size
    ]
    copyUpToLast: x [
        | i |
        i := self lastIndexOf: x.
        i = 0 ifTrue: [^self copy].
        ^self copyFrom: 1 to: i - 1
    ]
    copyAfterLast: x [
        | i |
        i := self lastIndexOf: x.
        i = 0 ifTrue: [^self copyFrom: 1 to: 0].
        ^self copyFrom: i + 1 to: self size
    ]
    take: n [ ^self copyFrom: 1 to: (n min: self size) ]
    drop: n [ ^self copyFrom: (n + 1 min: self size + 1) to: self size ]
    fourth [ ^self at: 4 ]
    middle [ ^self at: self size + 1 // 2 ]
    withIndexDo: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i) value: i] ]
    withIndexCollect: aBlock [ ^(1 to: self size) collect: [:i | aBlock value: (self at: i) value: i] ]
    pairsDo: aBlock [ 1 to: self size - 1 by: 2 do: [:i | aBlock value: (self at: i) value: (self at: i + 1)] ]
    isSorted [
        2 to: self size do: [:i | (self at: i - 1) <= (self at: i) ifFalse: [^false]].
        ^true
    ]
    atRandom [ ^self at: (Random new between: 1 and: self size) ]
    shuffled [
        | a j t r |
        r := Random new.
        a := self asArray copy.
        a size to: 2 by: -1 do: [:i |
            j := r between: 1 and: i.
            t := a at: i. a at: i put: (a at: j). a at: j put: t].
        ^a
    ]
    zip: other [
        ^(1 to: (self size min: other size)) collect: [:i | Array with: (self at: i) with: (other at: i)]
    ]
    cumulative [
        | s |
        s := 0.
        ^self collect: [:e | s := s + e]
    ]
    flatten [
        | r |
        r := OrderedCollection new.
        self do: [:e | (e isKindOf: Collection) & e isString not ifTrue: [r addAll: e flatten] ifFalse: [r add: e]].
        ^r asArray
    ]
    transpose [
        ^(1 to: self first size) collect: [:j | self collect: [:row | row at: j]]
    ]
    cartesianProduct: other [
        | r |
        r := OrderedCollection new.
        self do: [:a | other do: [:b | r add: (Array with: a with: b)]].
        ^r asArray
    ]
    combinations: k [
        | r |
        k = 0 ifTrue: [^Array with: Array new].
        self size < k ifTrue: [^Array new].
        r := OrderedCollection new.
        1 to: self size do: [:i |
            ((self copyFrom: i + 1 to: self size) combinations: k - 1) do: [:c |
                r add: (Array with: (self at: i)), c]].
        ^r asArray
    ]
    permutations [
        | r a rest |
        a := self asArray.
        a size <= 1 ifTrue: [^Array with: a].
        r := OrderedCollection new.
        1 to: a size do: [:i |
            rest := (a copyFrom: 1 to: i - 1), (a copyFrom: i + 1 to: a size).
            rest permutations do: [:p | r add: (Array with: (a at: i)), p]].
        ^r asArray
    ]
]

OrderedCollection extend [
    removeFirst: n [
        | r |
        r := OrderedCollection new.
        n timesRepeat: [r add: self removeFirst].
        ^r
    ]
    removeLast: n [
        | r |
        r := OrderedCollection new.
        n timesRepeat: [r addFirst: self removeLast].
        ^r
    ]
]

Set extend [
    union: o [ | r | r := self copy. r addAll: o. ^r ]
    intersect: o [ | r | r := self copyEmpty. self do: [:e | (o includes: e) ifTrue: [r add: e]]. ^r ]
    difference: o [ | r | r := self copyEmpty. self do: [:e | (o includes: e) ifFalse: [r add: e]]. ^r ]
    symmetricDifference: o [ ^(self difference: o) union: (o asSet difference: self) ]
    & o [ ^self intersect: o ]
    | o [ ^self union: o ]
    - o [ ^self difference: o ]
]

Dictionary extend [
    keysSorted [ ^self keys asSortedCollection asArray ]
    keysSortedSafely [ ^self keysSorted ]
    merge: other [ other keysAndValuesDo: [:k :v | self at: k put: v]. ^self ]
    invert [ | r | r := Dictionary new. self keysAndValuesDo: [:k :v | r at: v put: k]. ^r ]
    removeKeys: ks [ ks do: [:k | self removeKey: k ifAbsent: [nil]] ]
    associationAt: k [ ^k -> (self at: k) ]
]

String extend [
    asUppercaseFirst [ self isEmpty ifTrue: [^self copy]. ^(self first asUppercase asString), (self copyFrom: 2 to: self size) ]
    capitalized [ ^self asUppercaseFirst ]
    padLeft: c to: n [ ^self size >= n ifTrue: [self] ifFalse: [(String new: n - self size withAll: c), self] ]
    padRight: c to: n [ ^self size >= n ifTrue: [self] ifFalse: [self, (String new: n - self size withAll: c)] ]
    padLeftTo: n [ ^self padLeft: $  to: n ]
    padRightTo: n [ ^self padRight: $  to: n ]
    format: args [
        | ws i j key |
        ws := WriteStream on: String new.
        i := 1.
        [i <= self size] whileTrue: [
            (self at: i) = ${
                ifTrue: [
                    j := i + 1.
                    [j <= self size and: [(self at: j) ~= $}]] whileTrue: [j := j + 1].
                    key := self copyFrom: i + 1 to: j - 1.
                    (args isKindOf: Dictionary)
                        ifTrue: [ws nextPutAll: (args at: key asSymbol ifAbsent: [args at: key]) displayString]
                        ifFalse: [ws nextPutAll: (args at: key asInteger) displayString].
                    i := j + 1]
                ifFalse: [ws nextPut: (self at: i). i := i + 1]].
        ^ws contents
    ]
]

Number extend [
    degreesToRadians [ ^self * 0.017453292519943295 ]
    radiansToDegrees [ ^self * 57.29577951308232 ]
    min: a max: b [ ^(self min: a) max: b ]
    to: stop collect: aBlock [ ^(self to: stop) collect: aBlock ]
]

Integer extend [
    binomial: k [ ^self factorial // (k factorial * (self - k) factorial) ]
    fibonacci [
        | a b t |
        a := 0. b := 1.
        self timesRepeat: [t := a + b. a := b. b := t].
        ^a
    ]
    floorLog: b [
        | n r |
        n := self. r := 0.
        [n >= b] whileTrue: [n := n // b. r := r + 1].
        ^r
    ]
    printOn: aStream base: b [ aStream nextPutAll: (self printString: b) ]
    romanNumeral [
        | n ws |
        n := self.
        ws := WriteStream on: String new.
        #(1000 900 500 400 100 90 50 40 10 9 5 4 1) with: #('M' 'CM' 'D' 'CD' 'C' 'XC' 'L' 'XL' 'X' 'IX' 'V' 'IV' 'I') do: [:v :s |
            [n >= v] whileTrue: [ws nextPutAll: s. n := n - v]].
        ^ws contents
    ]
    printRoman [ ^self romanNumeral ]
    isPerfect [
        | s |
        s := 0.
        1 to: self - 1 do: [:i | self \\ i = 0 ifTrue: [s := s + i]].
        ^s = self and: [self > 1]
    ]
    digits [ ^self abs printString asArray collect: [:c | c digitValue] ]
    sumOfDigits [ ^self digits sum ]
    isSquare [ | r | self < 0 ifTrue: [^false]. r := self sqrt truncated. ^r * r = self ]
    isPowerOfTwo [ ^self > 0 and: [(self bitAnd: self - 1) = 0] ]
    highBit [ | n r | n := self. r := 0. [n > 0] whileTrue: [n := n // 2. r := r + 1]. ^r ]
    lowBit [ | n r | self = 0 ifTrue: [^0]. n := self. r := 1. [n \\ 2 = 0] whileTrue: [n := n // 2. r := r + 1]. ^r ]
    bitInvert [ ^-1 - self ]
    noMask: m [ ^(self bitAnd: m) = 0 ]
    anyMask: m [ ^(self bitAnd: m) ~= 0 ]
]

ReadStream extend [
    skipTo: x [
        [self atEnd] whileFalse: [self next = x ifTrue: [^true]].
        ^false
    ]
    detect: aBlock [
        [self atEnd] whileFalse: [ | e | e := self next. (aBlock value: e) ifTrue: [^e]].
        ^nil
    ]
    notEmpty [ ^self atEnd not ]
    back [ self position: self position - 1. ^self peek ]
    lines [
        | r |
        r := OrderedCollection new.
        [self atEnd] whileFalse: [r add: self nextLine].
        ^r asArray
    ]
    linesDo: aBlock [ self lines do: aBlock ]
]

WriteStream extend [
    crtab [ self nl; tab ]
    lf [ self nextPut: (Character value: 10) ]
    newLine [ self nl ]
    flush [ ^self ]
    close [ ^self ]
    print: x paddedWith: c to: n [ self nextPutAll: (x printString padLeft: c to: n) ]
]
