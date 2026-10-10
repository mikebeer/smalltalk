"streams.st - ReadStream, WriteStream, ReadWriteStream"

Stream extend [
    isStream [ ^true ]
    do: aBlock [ [self atEnd] whileFalse: [aBlock value: self next] ]
    nl [ self nextPut: Character nl ]
    cr [ self nl ]
    tab [ self nextPut: Character tab ]
    space [ self nextPut: Character space ]
    tab: n [ n timesRepeat: [self tab] ]
    space: n [ n timesRepeat: [self space] ]
    nl: n [ n timesRepeat: [self nl] ]
    print: x [ self nextPutAll: x printString ]
    display: x [ self nextPutAll: x displayString ]
    << x [ self nextPutAll: x displayString ]
    show: s [ self nextPutAll: s ]
    showCr: s [ self nextPutAll: s; nl ]
    displayNl: x [ self nextPutAll: x displayString; nl ]
    store: x [ self nextPutAll: x storeString ]
    next: n [
        | r |
        r := OrderedCollection new.
        n timesRepeat: [r add: self next].
        ^r asArray
    ]
    collect: aBlock [ | r | r := OrderedCollection new. self do: [:e | r add: (aBlock value: e)]. ^r readStream ]
    select: aBlock [ | r | r := OrderedCollection new. self do: [:e | (aBlock value: e) ifTrue: [r add: e]]. ^r readStream ]
    inject: acc into: aBlock [ | r | r := acc. self do: [:e | r := aBlock value: r value: e]. ^r ]
]

PositionableStream class extend [
    on: aCollection [ ^self basicNew on: aCollection ]
]

PositionableStream extend [
    | coll pos limit |
    on: aCollection [ coll := aCollection. pos := 0. limit := aCollection size ]
    atEnd [ ^pos >= limit ]
    notAtEnd [ ^pos < limit ]
    next [
        pos >= limit ifTrue: [^SystemExceptions.EndOfStream signalOn: self].
        pos := pos + 1.
        ^coll at: pos
    ]
    peek [ ^pos >= limit ifTrue: [nil] ifFalse: [coll at: pos + 1] ]
    peekFor: x [ (self atEnd not and: [self peek = x]) ifTrue: [pos := pos + 1. ^true]. ^false ]
    skip: n [ pos := ((pos + n) max: 0) min: limit ]
    skipSeparators [ [self atEnd not and: [self peek isSeparator]] whileTrue: [pos := pos + 1] ]
    position [ ^pos ]
    position: n [ pos := n ]
    reset [ pos := 0 ]
    setToEnd [ pos := limit ]
    size [ ^limit ]
    isEmpty [ ^limit = 0 ]
    contents [ ^coll copyFrom: 1 to: limit ]
    upToEnd [
        | r |
        r := coll copyFrom: pos + 1 to: limit.
        pos := limit.
        ^r
    ]
    upTo: x [
        | i r |
        i := pos + 1.
        [i <= limit and: [(coll at: i) ~= x]] whileTrue: [i := i + 1].
        r := coll copyFrom: pos + 1 to: i - 1.
        pos := i min: limit.
        ^r
    ]
    next: n [
        | r |
        pos + n > limit ifTrue: [^SystemExceptions.EndOfStream signalOn: self].
        r := coll copyFrom: pos + 1 to: pos + n.
        pos := pos + n.
        ^r
    ]
    nextLine [ | r | r := self upTo: Character nl. ^r ]
    collection [ ^coll ]
    readStream [ ^ReadStream on: self contents ]
    printOn: aStream [ aStream nextPutAll: self class article, ' ', self class name ]
]

ReadStream class extend [
    on: aCollection from: a to: b [ ^self on: (aCollection copyFrom: a to: b) ]
]

WriteStream class extend [
    on: aCollection [ ^self basicNew initOn: aCollection ]
    with: aCollection [ ^self basicNew initOn: aCollection; setContents: aCollection; yourself ]
]

WriteStream extend [
    | acc isStr |
    initOn: aCollection [
        coll := aCollection.
        pos := 0.
        limit := 0.
        isStr := aCollection isString.
        acc := aCollection copyEmpty.
    ]
    setContents: aCollection [ acc := aCollection copy ]
    nextPut: x [
        isStr
            ifTrue: [acc := acc , (String with: x)]
            ifFalse: [acc := acc copyWith: x].
        ^x
    ]
    nextPutAll: aColl [
        isStr
            ifTrue: [acc := acc , aColl asString]
            ifFalse: [acc := acc , aColl].
        ^aColl
    ]
    next: n put: x [ n timesRepeat: [self nextPut: x] ]
    contents [ ^acc ]
    reset [ acc := acc copyEmpty ]
    size [ ^acc size ]
    isEmpty [ ^acc isEmpty ]
    position [ ^acc size ]
    atEnd [ ^true ]
    readStream [ ^ReadStream on: acc ]
    emptyStream [ self reset ]
]

ReadWriteStream extend [
    next [
        pos >= acc size ifTrue: [^SystemExceptions.EndOfStream signalOn: self].
        pos := pos + 1.
        ^acc at: pos
    ]
    atEnd [ ^pos >= acc size ]
    peek [ ^pos >= acc size ifTrue: [nil] ifFalse: [acc at: pos + 1] ]
    upToEnd [ | r | r := acc copyFrom: pos + 1 to: acc size. pos := acc size. ^r ]
    reset [ pos := 0 ]
    position [ ^pos ]
    position: n [ pos := n ]
]

ReadWriteStream class extend [
    on: aCollection [ ^self basicNew initOn: aCollection ]
    with: aCollection [ ^self basicNew initOn: aCollection; setContents: aCollection; yourself ]
]

SequenceableCollection extend [
    readStream [ ^ReadStream on: self ]
    writeStream [ ^WriteStream on: self copyEmpty ]
    readWriteStream [ ^ReadWriteStream on: self copyEmpty ]
]

ReadWriteStream subclass: FileStream [
    | path mode |
    FileStream class >> open: aPath mode: aMode [
        | s text |
        s := self basicNew initOn: String new.
        s setPath: aPath mode: aMode.
        aMode = 'r' ifTrue: [
            text := self primRead: aPath.
            text isNil ifTrue: [^SystemExceptions.NotFound signalOn: aPath what: 'file' reason: 'could not open file ', aPath].
            s setContents: text].
        ^s
    ]
    FileStream class >> open: aPath [ ^self open: aPath mode: 'r' ]
    FileStream class >> read: aPath [ ^self open: aPath mode: 'r' ]
    FileStream class >> write: aPath [ ^self open: aPath mode: 'w' ]
    FileStream class >> append: aPath [ ^self open: aPath mode: 'a' ]
    FileStream class >> primRead: aPath [ <primitive: fileRead> ^nil ]
    FileStream class >> exists: aPath [ ^(self primRead: aPath) notNil ]
    FileStream class >> contentsOf: aPath [ ^self primRead: aPath ]
    setPath: aPath mode: aMode [ path := aPath. mode := aMode ]
    path [ ^path ]
    primWrite: p text: text append: flag [ <primitive: fileWrite> ^false ]
    flush [
        mode = 'r' ifTrue: [^self].
        (self primWrite: path text: acc append: mode = 'a') ifFalse: [^self error: 'could not write file ', path].
        mode := 'a'.
        acc := acc copyEmpty
    ]
    close [ self flush ]
    nextLine [
        | r c |
        self atEnd ifTrue: [^nil].
        r := WriteStream on: String new.
        [self atEnd or: [(c := self next) = Character nl]] whileFalse: [r nextPut: c].
        ^r contents
    ]
    lines [ | r | r := OrderedCollection new. [self atEnd] whileFalse: [r add: self nextLine]. ^r ]
    contents [ ^acc ]
]
