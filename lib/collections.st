"collections.st - Collection, Array, String, Symbol, OrderedCollection, Dictionary, Set, Bag, Interval"

Collection class extend [
    with: a [ ^self new add: a; yourself ]
    with: a with: b [ ^self new add: a; add: b; yourself ]
    with: a with: b with: c [ ^self new add: a; add: b; add: c; yourself ]
    with: a with: b with: c with: d [ ^self new add: a; add: b; add: c; add: d; yourself ]
    with: a with: b with: c with: d with: e [ ^self new add: a; add: b; add: c; add: d; add: e; yourself ]
    with: a with: b with: c with: d with: e with: f [ ^self new add: a; add: b; add: c; add: d; add: e; add: f; yourself ]
    withAll: aCollection [ ^self new addAll: aCollection; yourself ]
    from: anArray [ ^self withAll: anArray ]
]

Collection extend [
    isCollection [ ^true ]
    do: aBlock [ ^self subclassResponsibility ]
    do: aBlock separatedBy: sepBlock [
        | first |
        first := true.
        self do: [:e | first ifTrue: [first := false] ifFalse: [sepBlock value]. aBlock value: e]
    ]
    size [ | n | n := 0. self do: [:e | n := n + 1]. ^n ]
    isEmpty [ ^self size = 0 ]
    notEmpty [ ^self size > 0 ]
    isEmptyOrNil [ ^self isEmpty ]
    ifEmpty: aBlock [ ^self isEmpty ifTrue: [aBlock value] ifFalse: [self] ]
    ifNotEmpty: aBlock [ ^self isEmpty ifTrue: [self] ifFalse: [aBlock numArgs = 0 ifTrue: [aBlock value] ifFalse: [aBlock value: self]] ]
    ifEmpty: eBlock ifNotEmpty: nBlock [ ^self isEmpty ifTrue: [eBlock value] ifFalse: [nBlock numArgs = 0 ifTrue: [nBlock value] ifFalse: [nBlock value: self]] ]
    includes: x [ self do: [:e | e = x ifTrue: [^true]]. ^false ]
    identityIncludes: x [ self do: [:e | e == x ifTrue: [^true]]. ^false ]
    includesAll: aColl [ aColl do: [:e | (self includes: e) ifFalse: [^false]]. ^true ]
    includesAny: aColl [ aColl do: [:e | (self includes: e) ifTrue: [^true]]. ^false ]
    occurrencesOf: x [ | n | n := 0. self do: [:e | e = x ifTrue: [n := n + 1]]. ^n ]
    count: aBlock [ | n | n := 0. self do: [:e | (aBlock value: e) ifTrue: [n := n + 1]]. ^n ]
    detect: aBlock [ ^self detect: aBlock ifNone: [SystemExceptions.ElementNotFound signalOn: self what: 'object satisfying the condition' reason: 'Element not found'] ]
    detect: aBlock ifNone: noneBlock [ self do: [:e | (aBlock value: e) ifTrue: [^e]]. ^noneBlock value ]
    anySatisfy: aBlock [ self do: [:e | (aBlock value: e) ifTrue: [^true]]. ^false ]
    allSatisfy: aBlock [ self do: [:e | (aBlock value: e) ifFalse: [^false]]. ^true ]
    contains: aBlock [ ^self anySatisfy: aBlock ]
    conform: aBlock [ ^self allSatisfy: aBlock ]
    noneSatisfy: aBlock [ ^(self anySatisfy: aBlock) not ]
    anyOne [ self do: [:e | ^e]. ^self emptyCheck ]
    emptyCheck [ ^SystemExceptions.EmptyCollection signalOn: self ]
    inject: acc into: aBlock [ | r | r := acc. self do: [:e | r := aBlock value: r value: e]. ^r ]
    fold: aBlock [
        | r first |
        self isEmpty ifTrue: [^self emptyCheck].
        first := true.
        self do: [:e | first ifTrue: [r := e. first := false] ifFalse: [r := aBlock value: r value: e]].
        ^r
    ]
    detectMax: aBlock [
        | best bv |
        self do: [:e | | v | v := aBlock value: e. (bv isNil or: [v > bv]) ifTrue: [best := e. bv := v]].
        ^best
    ]
    detectMin: aBlock [
        | best bv |
        self do: [:e | | v | v := aBlock value: e. (bv isNil or: [v < bv]) ifTrue: [best := e. bv := v]].
        ^best
    ]
    max [ ^self fold: [:a :b | a max: b] ]
    min [ ^self fold: [:a :b | a min: b] ]
    sum [ ^self inject: 0 into: [:a :b | a + b] ]
    sum: aBlock [ ^self inject: 0 into: [:a :b | a + (aBlock value: b)] ]
    average [ ^self sum / self size ]
    collect: aBlock [ | r | r := OrderedCollection new. self do: [:e | r add: (aBlock value: e)]. ^r ]
    select: aBlock [ | r | r := self copyEmpty. self do: [:e | (aBlock value: e) ifTrue: [r add: e]]. ^r ]
    reject: aBlock [ ^self select: [:e | (aBlock value: e) not] ]
    copyEmpty [ ^self species new ]
    collect: aBlock thenSelect: sBlock [ ^(self collect: aBlock) select: sBlock ]
    add: x [ ^self subclassResponsibility ]
    addAll: aColl [ aColl do: [:e | self add: e]. ^aColl ]
    remove: x [ ^self remove: x ifAbsent: [SystemExceptions.NotFound signalOn: x what: 'object' reason: 'Object not found'] ]
    remove: x ifAbsent: aBlock [ ^self subclassResponsibility ]
    removeAll: aColl [ aColl do: [:e | self remove: e]. ^aColl ]
    asArray [ | a i | a := Array new: self size. i := 0. self do: [:e | a at: (i := i + 1) put: e]. ^a ]
    asOrderedCollection [ | r | r := OrderedCollection new. self do: [:e | r add: e]. ^r ]
    asBag [ | r | r := Bag new. self do: [:e | r add: e]. ^r ]
    asSet [ | r | r := Set new. self do: [:e | r add: e]. ^r ]
    asSortedCollection [ ^SortedCollection new addAll: self; yourself ]
    asSortedCollection: aBlock [ ^(SortedCollection sortBlock: aBlock) addAll: self; yourself ]
    asString [ ^self displayString ]
    sorted [ ^self asArray sort ]
    sorted: aBlock [ ^self asArray sort: aBlock ]
    asSortedArray [ ^self sorted ]
    readStream [ ^ReadStream on: self asArray ]
    printOn: aStream [
        aStream nextPutAll: self class storeString, ' ('.
        self do: [:e | aStream print: e; space].
        aStream nextPut: $)
    ]
    displayString [
        | s |
        s := WriteStream on: String new.
        s nextPutAll: self class storeString, ' ('.
        self do: [:e | s nextPutAll: e displayString; space].
        s nextPut: $).
        ^s contents
    ]
    displayNl [ Transcript showCr: self displayString ]
]

SequenceableCollection class extend [
    new: n withAll: x [ | c | c := self new: n. 1 to: n do: [:i | c at: i put: x]. ^c ]
]

SequenceableCollection extend [
    isSequenceable [ ^true ]
    do: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i)] ]
    doWithIndex: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i) value: i] ]
    keysAndValuesDo: aBlock [ 1 to: self size do: [:i | aBlock value: i value: (self at: i)] ]
    reverseDo: aBlock [ self size to: 1 by: -1 do: [:i | aBlock value: (self at: i)] ]
    from: a to: b do: aBlock [ a to: b do: [:i | aBlock value: (self at: i)] ]
    from: a to: b doWithIndex: aBlock [ a to: b do: [:i | aBlock value: (self at: i) value: i] ]
    with: other do: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i) value: (other at: i)] ]
    with: other collect: aBlock [
        | r |
        r := OrderedCollection new.
        1 to: self size do: [:i | r add: (aBlock value: (self at: i) value: (other at: i))].
        ^r asArray
    ]
    collectWithIndex: aBlock [ | r | r := OrderedCollection new. 1 to: self size do: [:i | r add: (aBlock value: (self at: i) value: i)]. ^r asArray ]
    isEmpty [ ^self size = 0 ]
    first [ ^self size = 0 ifTrue: [self emptyCheck] ifFalse: [self at: 1] ]
    last [ ^self size = 0 ifTrue: [self emptyCheck] ifFalse: [self at: self size] ]
    first: n [ ^self copyFrom: 1 to: n ]
    last: n [ ^self copyFrom: self size - n + 1 to: self size ]
    allButFirst [ ^self copyFrom: 2 to: self size ]
    allButFirst: n [ ^self copyFrom: n + 1 to: self size ]
    allButLast [ ^self copyFrom: 1 to: self size - 1 ]
    allButLast: n [ ^self copyFrom: 1 to: self size - n ]
    second [ ^self at: 2 ]
    third [ ^self at: 3 ]
    at: i ifAbsent: aBlock [ ^(i between: 1 and: self size) ifTrue: [self at: i] ifFalse: [aBlock value] ]
    indexOf: x [ ^self indexOf: x ifAbsent: [0] ]
    indexOf: x ifAbsent: aBlock [ 1 to: self size do: [:i | (self at: i) = x ifTrue: [^i]]. ^aBlock value ]
    identityIndexOf: x [ 1 to: self size do: [:i | (self at: i) == x ifTrue: [^i]]. ^0 ]
    includes: x [ 1 to: self size do: [:i | (self at: i) = x ifTrue: [^true]]. ^false ]
    copyFrom: a [ ^self copyFrom: a to: self size ]
    copyFrom: a to: b [
        | r |
        b < a ifTrue: [^self copyEmpty].
        (a < 1 or: [b > self size]) ifTrue: [^self checkIndex: (a < 1 ifTrue: [a] ifFalse: [b])].
        r := OrderedCollection new.
        a to: b do: [:i | r add: (self at: i)].
        ^self species withAll: r
    ]
    copyWith: x [ ^self species withAll: ((OrderedCollection withAll: self) add: x; yourself) ]
    copyWithout: x [ ^self reject: [:e | e = x] ]
    , other [ ^self species withAll: ((OrderedCollection withAll: self) addAll: other; yourself) ]
    reverse [ | r | r := OrderedCollection new. self reverseDo: [:e | r add: e]. ^self species withAll: r ]
    reversed [ ^self reverse ]
    copyReplaceAll: old with: new [
        | r i n |
        r := OrderedCollection new.
        i := 1. n := old size.
        [i <= self size] whileTrue: [
            (i + n - 1 <= self size and: [(self copyFrom: i to: i + n - 1) = old])
                ifTrue: [r addAll: new. i := i + n]
                ifFalse: [r add: (self at: i). i := i + 1]].
        ^self species withAll: r
    ]
    copyReplacing: old with: new [ ^self collect: [:e | e = old ifTrue: [new] ifFalse: [e]] ]
    swap: i with: j [ | t | t := self at: i. self at: i put: (self at: j). self at: j put: t ]
    atAll: indices [ ^indices collect: [:i | self at: i] ]
    atAll: indices put: x [ indices do: [:i | self at: i put: x] ]
    atAllPut: x [ 1 to: self size do: [:i | self at: i put: x] ]
    replaceFrom: a to: b with: coll startingAt: s [ | k | k := s. a to: b do: [:i | self at: i put: (coll at: k). k := k + 1] ]
    replaceFrom: a to: b with: coll [ self replaceFrom: a to: b with: coll startingAt: 1 ]
    startsWith: prefix [ ^self size >= prefix size and: [(self copyFrom: 1 to: prefix size) = prefix] ]
    endsWith: suffix [ ^self size >= suffix size and: [(self copyFrom: self size - suffix size + 1 to: self size) = suffix] ]
    = other [
        self class == other class ifFalse: [^false].
        self size = other size ifFalse: [^false].
        1 to: self size do: [:i | (self at: i) = (other at: i) ifFalse: [^false]].
        ^true
    ]
    hash [ | h | h := self size. self do: [:e | h := (h * 31 + e hash) \\ 1073741823]. ^h ]
    readStream [ ^ReadStream on: self ]
    writeStream [ ^WriteStream on: self copyEmpty ]
    sort [ ^self sort: [:a :b | a <= b] ]
    sort: aBlock [
        | a |
        a := self asArray.
        a := a mergeSort: aBlock.
        1 to: self size do: [:i | self at: i put: (a at: i)]
    ]
    mergeSort: aBlock [
        | n mid l r res i j k |
        n := self size.
        n <= 1 ifTrue: [^self copy].
        mid := n // 2.
        l := (self copyFrom: 1 to: mid) mergeSort: aBlock.
        r := (self copyFrom: mid + 1 to: n) mergeSort: aBlock.
        res := Array new: n.
        i := 1. j := 1. k := 1.
        [i <= l size and: [j <= r size]] whileTrue: [
            (aBlock value: (l at: i) value: (r at: j))
                ifTrue: [res at: k put: (l at: i). i := i + 1]
                ifFalse: [res at: k put: (r at: j). j := j + 1].
            k := k + 1].
        [i <= l size] whileTrue: [res at: k put: (l at: i). i := i + 1. k := k + 1].
        [j <= r size] whileTrue: [res at: k put: (r at: j). j := j + 1. k := k + 1].
        ^res
    ]
    asSortedArray [ ^self asArray mergeSort: [:a :b | a <= b] ]
    sorted [ ^self asArray mergeSort: [:a :b | a <= b] ]
    sorted: aBlock [ ^self asArray mergeSort: aBlock ]
    asSortedCollection [ ^SortedCollection new addAll: self; yourself ]
    keys [ ^1 to: self size ]
    isPalindrome [ ^self = self reverse ]
]

ArrayedCollection class extend [
    new: n withAll: x [ | c | c := self new: n. 1 to: n do: [:i | c at: i put: x]. ^c ]
    withAll: aColl [
        | c i |
        c := self new: aColl size.
        i := 0.
        aColl do: [:e | c at: (i := i + 1) put: e].
        ^c
    ]
    with: a [ ^(self new: 1) at: 1 put: a; yourself ]
    with: a with: b [ ^(self new: 2) at: 1 put: a; at: 2 put: b; yourself ]
    with: a with: b with: c [ ^(self new: 3) at: 1 put: a; at: 2 put: b; at: 3 put: c; yourself ]
    with: a with: b with: c with: d [ ^(self new: 4) at: 1 put: a; at: 2 put: b; at: 3 put: c; at: 4 put: d; yourself ]
    with: a with: b with: c with: d with: e [ ^(self new: 5) at: 1 put: a; at: 2 put: b; at: 3 put: c; at: 4 put: d; at: 5 put: e; yourself ]
    with: a with: b with: c with: d with: e with: f [ ^(self new: 6) at: 1 put: a; at: 2 put: b; at: 3 put: c; at: 4 put: d; at: 5 put: e; at: 6 put: f; yourself ]
    from: anArray [ ^self withAll: anArray ]
]

ArrayedCollection extend [
    add: x [ ^self shouldNotImplement ]
    copyEmpty [ ^self species new: 0 ]
    collect: aBlock [
        | r |
        r := self species new: self size.
        1 to: self size do: [:i | r at: i put: (aBlock value: (self at: i))].
        ^r
    ]
    select: aBlock [
        | r |
        r := OrderedCollection new.
        self do: [:e | (aBlock value: e) ifTrue: [r add: e]].
        ^self species withAll: r
    ]
    reverse [ | r n | n := self size. r := self species new: n. 1 to: n do: [:i | r at: i put: (self at: n - i + 1)]. ^r ]
    , other [ | r n | n := self size. r := self species new: n + other size. 1 to: n do: [:i | r at: i put: (self at: i)]. other do: [:e | r at: (n := n + 1) put: e]. ^r ]
    printOn: aStream [
        aStream nextPutAll: self class storeString, ' ('.
        self do: [:e | aStream print: e; space].
        aStream nextPut: $)
    ]
]

Array class extend [
    new [ ^self new: 0 ]
]

Array extend [
    isArray [ ^true ]
    at: i [ <primitive: arrAt> ^self checkIndex: i ]
    at: i put: v [ <primitive: arrAtPut> ^self checkIndex: i ]
    size [ <primitive: arrSize> ]
    basicSize [ <primitive: arrSize> ]
    do: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i)] ]
    copyFrom: a to: b [ <primitive: arrCopy> ^super copyFrom: a to: b ]
    shallowCopy [ <primitive: shallowCopy> ]
    replaceFrom: a to: b with: coll startingAt: s [ <primitive: arrReplace> ^super replaceFrom: a to: b with: coll startingAt: s ]
    atAllPut: x [ <primitive: arrFill> ]
    deepCopy [ ^self collect: [:e | e copy] ]
    , other [
        other class == Array ifFalse: [other isCollection ifFalse: [^self copyWith: other]].
        ^super , other
    ]
    printOn: aStream [
        aStream nextPut: $(.
        self do: [:e | aStream print: e; space].
        aStream nextPut: $)
    ]
    displayString [
        | s |
        s := WriteStream on: String new.
        s nextPut: $(.
        self do: [:e | s nextPutAll: e displayString; space].
        s nextPut: $).
        ^s contents
    ]
    storeString [ ^self printString ]
    isLiteralArray [ ^true ]
]

String class extend [
    new [ ^self new: 0 ]
    new: n [ <primitive: basicNewSize> ]
    new: n withAll: c [ | s | s := self new: n. 1 to: n do: [:i | s at: i put: c]. ^s ]
    with: a [ ^(self new: 1) at: 1 put: a; yourself ]
    cr [ ^String with: Character cr ]
    nl [ ^String with: Character nl ]
    tab [ ^String with: Character tab ]
    lf [ ^String with: Character nl ]
    fromString: s [ ^s copy ]
]

CharacterArray extend [
    isString [ <primitive: isString> ]
]

String extend [
    size [ <primitive: strSize> ]
    basicSize [ <primitive: strSize> ]
    at: i [ <primitive: strAt> ^self checkIndex: i ]
    at: i put: c [ <primitive: strAtPut> ^self checkIndex: i ]
    , s [ <primitive: strCat> ^self , s displayString ]
    copyFrom: a to: b [ <primitive: strCopy> ^super copyFrom: a to: b ]
    copyFrom: a [ ^self copyFrom: a to: self size ]
    = s [ <primitive: strEq> ]
    < s [ <primitive: strLt> ^self error: 'argument must be a string' ]
    > s [ ^s < self ]
    <= s [ ^(s < self) not ]
    >= s [ ^(self < s) not ]
    hash [ <primitive: strHash> ]
    sameAs: s [ ^self asLowercase = s asLowercase ]
    caseSensitiveCompareTo: s [ <primitive: strCompare> ]
    max: s [ ^self < s ifTrue: [s] ifFalse: [self] ]
    min: s [ ^self < s ifTrue: [self] ifFalse: [s] ]
    isString [ ^true ]
    isSymbol [ ^false ]
    isEmpty [ ^self size = 0 ]
    notEmpty [ ^self size > 0 ]
    do: aBlock [ 1 to: self size do: [:i | aBlock value: (self at: i)] ]
    asString [ ^self ]
    asSymbol [ <primitive: asSymbol> ]
    asNumber [ <primitive: asNumber> ]
    asInteger [ | n | n := self asNumber. ^n isInteger ifTrue: [n] ifFalse: [nil] ]
    asUppercase [ <primitive: upper> ]
    asLowercase [ <primitive: lower> ]
    asUppercaseFirst [ self isEmpty ifTrue: [^self]. ^(String with: (self at: 1) asUppercase), (self copyFrom: 2) ]
    first [ ^self at: 1 ]
    last [ ^self at: self size ]
    reverse [ <primitive: strReverse> ]
    reversed [ ^self reverse ]
    trimSeparators [ <primitive: strTrim> ]
    subStrings [ <primitive: strSplit> ]
    substrings [ ^self subStrings ]
    subStrings: seps [ <primitive: strSplit> ]
    substrings: seps [ ^self subStrings: seps ]
    tokenize: seps [ ^self subStrings: seps ]
    lines [ ^(self subStrings: (String with: Character nl)) ]
    indexOf: c [ <primitive: strIndexOf> ^0 ]
    indexOf: c ifAbsent: aBlock [ | i | i := self indexOf: c. ^i = 0 ifTrue: [aBlock value] ifFalse: [i] ]
    includesSubstring: s [ ^(self indexOfSubCollection: s) > 0 ]
    includesSubstring: s caseSensitive: b [ ^b ifTrue: [self includesSubstring: s] ifFalse: [self asLowercase includesSubstring: s asLowercase] ]
    includes: c [ c isCharacter ifFalse: [^false]. ^(self indexOf: c) > 0 ]
    indexOfSubCollection: s [ <primitive: strIndexOf> ^0 ]
    indexOfSubCollection: s startingAt: i [ <primitive: strIndexOf> ^0 ]
    occurrencesOfString: s [
        | n i |
        n := 0. i := 1.
        [(i := self indexOfSubCollection: s startingAt: i) > 0] whileTrue: [n := n + 1. i := i + s size].
        ^n
    ]
    startsWith: s [ ^self size >= s size and: [(self copyFrom: 1 to: s size) = s] ]
    endsWith: s [ ^self size >= s size and: [(self copyFrom: self size - s size + 1) = s] ]
    replaceAll: old with: new [ 1 to: self size do: [:i | (self at: i) = old ifTrue: [self at: i put: new]] ]
    copyReplaceAll: old with: new [ <primitive: strReplaceAll> ^super copyReplaceAll: old with: new ]
    copyReplacing: old with: new [ ^self collect: [:c | c = old ifTrue: [new] ifFalse: [c]] ]
    replaceString: old withString: new [ ^self copyReplaceAll: old with: new ]
    copyWith: c [ ^self , (String with: c) ]
    asArray [ | a | a := Array new: self size. 1 to: self size do: [:i | a at: i put: (self at: i)]. ^a ]
    asBag [ ^self asArray asBag ]
    asSet [ ^self asArray asSet ]
    asOrderedCollection [ ^self asArray asOrderedCollection ]
    asByteArray [ ^self collect: [:c | c value] ]
    isNumeric [ ^self notEmpty and: [self allSatisfy: [:c | c isDigit]] ]
    first: n [ ^self copyFrom: 1 to: n ]
    last: n [ ^self copyFrom: self size - n + 1 ]
    displayString [ ^self copy ]
    displayNl [ Transcript showCr: self ]
    printNl [ Transcript showCr: self printString ]
    printString [ ^'''', (self copyReplaceAll: '''' with: ''''''), '''' ]
    printOn: aStream [ aStream nextPutAll: self printString ]
    displayOn: aStream [ aStream nextPutAll: self ]
    storeString [ ^self printString ]
    shallowCopy [ <primitive: shallowCopy> ]
    deepCopy [ ^self copy ]
    species [ ^String ]
    asLowercase: x [ ^self asLowercase ]
    bindWith: a [ ^self copyReplaceAll: '%1' with: a displayString ]
    bindWith: a with: b [ ^(self bindWith: a) copyReplaceAll: '%2' with: b displayString ]
    % args [
        | r |
        r := self.
        1 to: args size do: [:i | r := r copyReplaceAll: '%', i printString with: (args at: i) displayString].
        ^r
    ]
    asSymbol: x [ ^self asSymbol ]
    numArgs [ ^self asSymbol numArgs ]
    isPalindrome [ ^self = self reverse ]
    asCharacter [ ^self at: 1 ]
    join: aCollection [
        | ws first |
        ws := WriteStream on: String new.
        first := true.
        aCollection do: [:e | first ifFalse: [ws nextPutAll: self]. first := false. ws nextPutAll: e displayString].
        ^ws contents
    ]
    isCharacterArray [ ^true ]
    isLiteralString [ ^true ]
    keysAndValuesDo: aBlock [ 1 to: self size do: [:i | aBlock value: i value: (self at: i)] ]
]

Symbol class extend [
    new [ ^self shouldNotImplement ]
    new: n [ ^self shouldNotImplement ]
    with: a [ ^(String with: a) asSymbol ]
]

Symbol extend [
    isSymbol [ ^true ]
    isString [ ^true ]
    asSymbol [ ^self ]
    asString [ <primitive: asString> ]
    = other [ ^self == other ]
    hash [ <primitive: strHash> ]
    at: i put: c [ ^self shouldNotImplement ]
    species [ ^String ]
    copy [ ^self ]
    shallowCopy [ ^self ]
    deepCopy [ ^self ]
    displayString [ ^self asString ]
    displayNl [ Transcript showCr: self asString ]
    printString [
        | simple |
        self size = 0 ifTrue: [^'#''''' ].
        simple := ((self at: 1) isLetter or: [(self at: 1) = $_])
            ifTrue: [self allSatisfy: [:c | c isAlphaNumeric or: [c = $: or: [c = $_]]]]
            ifFalse: [self allSatisfy: [:c | '+-*/\<>=~@%|&?,' includes: c]].
        ^simple ifTrue: ['#', self asString] ifFalse: ['#''', self asString, '''']
    ]
    printOn: aStream [ aStream nextPutAll: self printString ]
    storeString [ ^self printString ]
    , s [ ^self asString , s ]
    numArgs [
        | n |
        (self size > 0 and: [(self at: 1) isLetter or: [(self at: 1) = $_]]) ifFalse: [^1].
        n := 0.
        self do: [:c | c = $: ifTrue: [n := n + 1]].
        ^n
    ]
    keywords [ ^(self asString subStrings: ':') collect: [:k | k , ':'] ]
    value: anObject [ ^anObject perform: self ]
    value: a value: b [ ^a perform: self with: b ]
    asUppercase [ ^self asString asUppercase asSymbol ]
    asLowercase [ ^self asString asLowercase asSymbol ]
]

OrderedCollection class extend [
    new [ ^self basicNew setUp: 16 ]
    new: n [ ^self basicNew setUp: (n max: 4) ]
    withAll: aColl [ ^self new addAll: aColl; yourself ]
]

OrderedCollection extend [
    | arr first last |
    setUp: n [ arr := Array new: n. first := n // 2 max: 1. last := first - 1 ]
    size [ ^last - first + 1 ]
    isEmpty [ ^last < first ]
    notEmpty [ ^last >= first ]
    capacity [ ^arr size ]
    grow [
        | n new off |
        n := arr size.
        new := Array new: n * 2.
        off := n // 2.
        new replaceFrom: first + off to: last + off with: arr startingAt: first.
        arr := new.
        first := first + off.
        last := last + off
    ]
    at: i [
        (i isInteger and: [i >= 1 and: [i <= self size]]) ifFalse: [^self checkIndex: i].
        ^arr at: first + i - 1
    ]
    at: i put: v [
        (i isInteger and: [i >= 1 and: [i <= self size]]) ifFalse: [^self checkIndex: i].
        ^arr at: first + i - 1 put: v
    ]
    primAdd: x [
        last = arr size ifTrue: [self grow].
        last := last + 1.
        arr at: last put: x.
        ^x
    ]
    add: x [ ^self primAdd: x ]
    addLast: x [ ^self primAdd: x ]
    addFirst: x [
        first = 1 ifTrue: [self grow].
        first := first - 1.
        arr at: first put: x.
        ^x
    ]
    addAll: aColl [ aColl do: [:e | self add: e]. ^aColl ]
    addAllFirst: aColl [ aColl reverseDo: [:e | self addFirst: e]. ^aColl ]
    add: x beforeIndex: i [
        | n |
        n := self size.
        (i between: 1 and: n + 1) ifFalse: [^self checkIndex: i].
        self primAdd: x.
        n to: i by: -1 do: [:k | arr at: first + k put: (arr at: first + k - 1)].
        arr at: first + i - 1 put: x.
        ^x
    ]
    insert: x before: i [ ^self add: x beforeIndex: i ]
    removeFirst [
        | x |
        last < first ifTrue: [^SystemExceptions.EmptyCollection signalOn: self].
        x := arr at: first.
        arr at: first put: nil.
        first := first + 1.
        ^x
    ]
    removeLast [
        | x |
        last < first ifTrue: [^SystemExceptions.EmptyCollection signalOn: self].
        x := arr at: last.
        arr at: last put: nil.
        last := last - 1.
        ^x
    ]
    removeFirst: n [ | r | r := OrderedCollection new. n timesRepeat: [r add: self removeFirst]. ^r ]
    removeLast: n [ | r | r := OrderedCollection new. n timesRepeat: [r addFirst: self removeLast]. ^r ]
    removeIndex: i [
        | x |
        (i between: 1 and: self size) ifFalse: [^self checkIndex: i].
        x := arr at: first + i - 1.
        i to: self size - 1 do: [:k | arr at: first + k - 1 put: (arr at: first + k)].
        arr at: last put: nil.
        last := last - 1.
        ^x
    ]
    removeAtIndex: i [ ^self removeIndex: i ]
    remove: x ifAbsent: aBlock [
        1 to: self size do: [:i | (arr at: first + i - 1) = x ifTrue: [self removeIndex: i. ^x]].
        ^aBlock value
    ]
    removeAll: aColl [ aColl do: [:e | self remove: e]. ^aColl ]
    removeAll [ self setUp: 16 ]
    removeAllSuchThat: aBlock [
        | keep |
        keep := OrderedCollection new.
        self do: [:e | (aBlock value: e) ifFalse: [keep add: e]].
        self setUp: (self size max: 16).
        self addAll: keep
    ]
    do: aBlock [
        | i |
        i := first.
        [i <= last] whileTrue: [aBlock value: (arr at: i). i := i + 1]
    ]
    first [ ^last < first ifTrue: [self emptyCheck] ifFalse: [arr at: first] ]
    last [ ^last < first ifTrue: [self emptyCheck] ifFalse: [arr at: last] ]
    copyEmpty [ ^self species new ]
    collect: aBlock [ | r | r := OrderedCollection new. self do: [:e | r add: (aBlock value: e)]. ^r ]
    select: aBlock [ | r | r := self copyEmpty. self do: [:e | (aBlock value: e) ifTrue: [r add: e]]. ^r ]
    reverse [ | r | r := self copyEmpty. self reverseDo: [:e | r add: e]. ^r ]
    copyFrom: a to: b [
        | r |
        b < a ifTrue: [^self copyEmpty].
        (a < 1 or: [b > self size]) ifTrue: [^self checkIndex: (a < 1 ifTrue: [a] ifFalse: [b])].
        r := self copyEmpty.
        a to: b do: [:i | r add: (self at: i)].
        ^r
    ]
    , other [ | r | r := self copyEmpty. r addAll: self. r addAll: other. ^r ]
    copyWith: x [ ^self copy add: x; yourself ]
    shallowCopy [ | r | r := self copyEmpty. r addAll: self. ^r ]
    deepCopy [ | r | r := self copyEmpty. self do: [:e | r add: e copy]. ^r ]
    asArray [ | a | a := Array new: self size. 1 to: self size do: [:i | a at: i put: (arr at: first + i - 1)]. ^a ]
    asOrderedCollection [ ^self ]
    printOn: aStream [
        aStream nextPutAll: self class storeString, ' ('.
        self do: [:e | aStream print: e; space].
        aStream nextPut: $)
    ]
    sort: aBlock [
        | a |
        a := self asArray mergeSort: aBlock.
        1 to: a size do: [:i | arr at: first + i - 1 put: (a at: i)]
    ]
    sort [ self sort: [:a :b | a <= b] ]
]

SortedCollection class extend [
    new [ ^self basicNew setUp: 16; sortBlock: [:a :b | a <= b]; yourself ]
    new: n [ ^self basicNew setUp: (n max: 4); sortBlock: [:a :b | a <= b]; yourself ]
    sortBlock: aBlock [ ^self new sortBlock: aBlock; yourself ]
]

SortedCollection extend [
    | sortBlock |
    sortBlock [ ^sortBlock ]
    sortBlock: aBlock [ sortBlock := aBlock. self resort ]
    resort [ self isEmpty ifFalse: [| a | a := self asArray mergeSort: sortBlock. self removeAll. a do: [:e | super add: e]] ]
    add: x [
        | i |
        i := self size.
        [i >= 1 and: [(sortBlock value: (self at: i) value: x) not]] whileTrue: [i := i - 1].
        super add: x beforeIndex: i + 1.
        ^x
    ]
    addFirst: x [ ^self shouldNotImplement ]
    addLast: x [ ^self shouldNotImplement ]
    at: i put: x [ ^self shouldNotImplement ]
    add: x beforeIndex: i [ ^self shouldNotImplement ]
    median [ ^self at: self size + 1 // 2 ]
    collect: aBlock [ | r | r := OrderedCollection new. self do: [:e | r add: (aBlock value: e)]. ^r ]
    copyEmpty [ ^SortedCollection sortBlock: sortBlock ]
    reverse [ | r | r := OrderedCollection new. self reverseDo: [:e | r add: e]. ^r ]
    , other [ | r | r := self copy. r addAll: other. ^r ]
    asSortedCollection [ ^self ]
]

Interval class extend [
    from: a to: b [ ^self from: a to: b by: 1 ]
    from: a to: b by: s [ ^self basicNew setFrom: a to: b by: s ]
]

Interval extend [
    | first last step |
    setFrom: a to: b by: s [ first := a. last := b. step := s ]
    first [ ^first ]
    last [ ^first + (self size - 1 * step) ]
    increment [ ^step ]
    size [
        step > 0 ifTrue: [^last >= first ifTrue: [(last - first) // step + 1] ifFalse: [0]].
        ^first >= last ifTrue: [(first - last) // step negated + 1] ifFalse: [0]
    ]
    isEmpty [ ^self size = 0 ]
    at: i [
        (i isInteger and: [i >= 1 and: [i <= self size]]) ifFalse: [^self checkIndex: i].
        ^first + (i - 1 * step)
    ]
    at: i put: v [ ^self shouldNotImplement ]
    do: aBlock [
        | i |
        i := first.
        step > 0
            ifTrue: [[i <= last] whileTrue: [aBlock value: i. i := i + step]]
            ifFalse: [[i >= last] whileTrue: [aBlock value: i. i := i + step]]
    ]
    collect: aBlock [
        | r i |
        r := Array new: self size.
        i := 0.
        self do: [:e | r at: (i := i + 1) put: (aBlock value: e)].
        ^r
    ]
    select: aBlock [ | r | r := OrderedCollection new. self do: [:e | (aBlock value: e) ifTrue: [r add: e]]. ^r asArray ]
    reverse [ ^Interval from: self last to: first by: step negated ]
    species [ ^Array ]
    copyEmpty [ ^Array new: 0 ]
    = other [ ^(other isKindOf: SequenceableCollection) and: [self asArray = other asArray] ]
    hash [ ^self asArray hash ]
    printOn: aStream [
        aStream nextPut: $(.
        self do: [:e | aStream print: e; space].
        aStream nextPut: $)
    ]
    displayString [ ^self printString ]
    copyFrom: a to: b [ ^self asArray copyFrom: a to: b ]
    asArray [ | a i | a := Array new: self size. i := 0. self do: [:e | a at: (i := i + 1) put: e]. ^a ]
]

HashedCollection class extend [
    new [ ^self basicNew ]
    new: n [ ^self basicNew ]
]

Dictionary class extend [
    new [ <primitive: dictNew> ]
    new: n [ <primitive: dictNew> ]
    withAll: aColl [ ^self new addAll: aColl; yourself ]
]

Dictionary extend [
    isDictionary [ ^true ]
    primAt: k [ <primitive: dictAt> ^SystemExceptions.NotFound signalOn: k what: 'key' reason: 'key not found' ]
    primHas: k [ <primitive: dictHas> ]
    primKeys [ <primitive: dictKeys> ]
    primValues [ <primitive: dictValues> ]
    at: k [ <primitive: dictAt> ^SystemExceptions.NotFound signalOn: k what: 'key' reason: 'key not found' ]
    at: k put: v [ <primitive: dictAtPut> ]
    at: k ifAbsent: aBlock [ ^(self primHas: k) ifTrue: [self primAt: k] ifFalse: [aBlock value] ]
    at: k ifAbsentPut: aBlock [ ^(self primHas: k) ifTrue: [self primAt: k] ifFalse: [self at: k put: aBlock value] ]
    at: k ifPresent: aBlock [ ^(self primHas: k) ifTrue: [aBlock value: (self primAt: k)] ifFalse: [nil] ]
    at: k ifPresent: pBlock ifAbsent: aBlock [ ^(self primHas: k) ifTrue: [pBlock value: (self primAt: k)] ifFalse: [aBlock value] ]
    add: assoc [ self at: assoc key put: assoc value. ^assoc ]
    addAll: aColl [
        aColl isDictionary
            ifTrue: [aColl keysAndValuesDo: [:k :v | self at: k put: v]]
            ifFalse: [aColl do: [:a | self add: a]].
        ^aColl
    ]
    includesKey: k [ <primitive: dictHas> ]
    includes: v [ self do: [:e | e = v ifTrue: [^true]]. ^false ]
    includesAssociation: a [ ^(self includesKey: a key) and: [(self at: a key) = a value] ]
    removeKey: k [ <primitive: dictRemove> ^SystemExceptions.NotFound signalOn: k what: 'key' reason: 'key not found' ]
    removeKey: k ifAbsent: aBlock [ ^(self primHas: k) ifTrue: [self removeKey: k] ifFalse: [aBlock value] ]
    remove: assoc [ ^self removeKey: assoc key ]
    remove: x ifAbsent: aBlock [ ^self shouldNotImplement ]
    size [ <primitive: dictSize> ]
    isEmpty [ ^self size = 0 ]
    keys [ ^self primKeys asSet ]
    values [ ^self primValues ]
    keysSortedSafe [ ^self primKeys sorted ]
    associations [ | ks | ks := self primKeys. ^ks collect: [:k | Association key: k value: (self primAt: k)] ]
    keyAtValue: v [ ^self keyAtValue: v ifAbsent: [nil] ]
    keyAtValue: v ifAbsent: aBlock [ self keysAndValuesDo: [:k :x | x = v ifTrue: [^k]]. ^aBlock value ]
    do: aBlock [ self primValues do: aBlock ]
    keysDo: aBlock [ self primKeys do: aBlock ]
    valuesDo: aBlock [ self primValues do: aBlock ]
    associationsDo: aBlock [ self associations do: aBlock ]
    keysAndValuesDo: aBlock [ | ks | ks := self primKeys. ks do: [:k | aBlock value: k value: (self primAt: k)] ]
    collect: aBlock [ | b | b := Bag new. self do: [:e | b add: (aBlock value: e)]. ^b ]
    select: aBlock [
        | r |
        r := self copyEmpty.
        self keysAndValuesDo: [:k :v | (aBlock value: v) ifTrue: [r at: k put: v]].
        ^r
    ]
    reject: aBlock [ ^self select: [:v | (aBlock value: v) not] ]
    keysSelect: aBlock [ | r | r := self copyEmpty. self keysAndValuesDo: [:k :v | (aBlock value: k) ifTrue: [r at: k put: v]]. ^r ]
    associationsSelect: aBlock [ | r | r := self copyEmpty. self keysAndValuesDo: [:k :v | (aBlock value: (k -> v)) ifTrue: [r at: k put: v]]. ^r ]
    detect: aBlock ifNone: noneBlock [ self do: [:e | (aBlock value: e) ifTrue: [^e]]. ^noneBlock value ]
    anyOne [ self do: [:e | ^e]. ^self emptyCheck ]
    copyEmpty [ ^self species new ]
    shallowCopy [ <primitive: dictCopy> ]
    deepCopy [ | r | r := self copyEmpty. self keysAndValuesDo: [:k :v | r at: k put: v copy]. ^r ]
    removeAll [ self primKeys do: [:k | self removeKey: k] ]
    asOrderedCollection [ ^self primValues asOrderedCollection ]
    asArray [ ^self primValues ]
    asBag [ ^self primValues asBag ]
    asSet [ ^self primValues asSet ]
    = other [
        (other isKindOf: Dictionary) ifFalse: [^false].
        self size = other size ifFalse: [^false].
        self keysAndValuesDo: [:k :v | ((other includesKey: k) and: [(other at: k) = v]) ifFalse: [^false]].
        ^true
    ]
    hash [ ^self size ]
    printOn: aStream [
        aStream nextPutAll: self class article, ' ', self class name, ' ('.
        self keysAndValuesDo: [:k :v | aStream print: k; nextPutAll: '->'; print: v; space].
        aStream nextPut: $)
    ]
    displayString [
        | s |
        s := WriteStream on: String new.
        s nextPutAll: self class article, ' ', self class name, ' ('.
        self keysAndValuesDo: [:k :v | s nextPutAll: k displayString; nextPutAll: '->'; nextPutAll: v displayString; space].
        s nextPut: $).
        ^s contents
    ]
]

Set extend [
    primPut: k with: v [ <primitive: dictAtPut> ]
    add: x [ self primPut: x with: x. ^x ]
    includes: x [ <primitive: dictHas> ]
    remove: x ifAbsent: aBlock [ ^(self includes: x) ifTrue: [self primRemove: x. x] ifFalse: [aBlock value] ]
    primRemove: x [ <primitive: dictRemove> ]
    size [ <primitive: dictSize> ]
    primKeys [ <primitive: dictKeys> ]
    do: aBlock [ self primKeys do: aBlock ]
    asArray [ ^self primKeys ]
    asSet [ ^self ]
    collect: aBlock [ | r | r := self copyEmpty. self do: [:e | r add: (aBlock value: e)]. ^r ]
    copyEmpty [ ^self species new ]
    shallowCopy [ <primitive: dictCopy> ]
    anyOne [ self do: [:e | ^e]. ^self emptyCheck ]
    = other [
        (other isKindOf: Set) ifFalse: [^false].
        self size = other size ifFalse: [^false].
        self do: [:e | (other includes: e) ifFalse: [^false]].
        ^true
    ]
    hash [ ^self size ]
    removeAll [ self primKeys do: [:k | self primRemove: k] ]
]

Set class extend [
    new [ <primitive: dictNew> ]
    new: n [ <primitive: dictNew> ]
]

Bag class extend [
    new [ ^self basicNew setUp ]
    new: n [ ^self basicNew setUp ]
]

Bag extend [
    | counts |
    setUp [ counts := Dictionary new ]
    add: x [ ^self add: x withOccurrences: 1 ]
    add: x withOccurrences: n [ counts at: x put: (counts at: x ifAbsent: [0]) + n. ^x ]
    occurrencesOf: x [ ^counts at: x ifAbsent: [0] ]
    includes: x [ ^counts includesKey: x ]
    remove: x ifAbsent: aBlock [
        | n |
        n := counts at: x ifAbsent: [^aBlock value].
        n = 1 ifTrue: [counts removeKey: x] ifFalse: [counts at: x put: n - 1].
        ^x
    ]
    size [ ^counts inject: 0 into: [:a :b | a + b] ]
    do: aBlock [ counts keysAndValuesDo: [:k :n | n timesRepeat: [aBlock value: k]] ]
    asSet [ ^counts keys ]
    sortedByCount [
        ^(counts associations collect: [:a | a value -> a key]) asSortedCollection: [:a :b | a key >= b key]
    ]
    copyEmpty [ ^Bag new ]
    shallowCopy [ | b | b := Bag new. self do: [:e | b add: e]. ^b ]
]

IdentitySet class extend [ new [ <primitive: dictNew> ] ]
IdentityDictionary class extend [ new [ <primitive: dictNew> ] ]
