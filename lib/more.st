"more.st - ByteArray, Duration, dependents, Rectangle, CompiledMethod, allInstances"

Array subclass: ByteArray [
    ByteArray class >> new: n [ ^(super new: n) atAllPut: 0; yourself ]
    ByteArray class >> new [ ^self new: 0 ]
    at: i put: v [
        (v isInteger and: [v >= 0 and: [v <= 255]]) ifFalse: [^self error: 'byte value expected'].
        ^super at: i put: v
    ]
    collect: aBlock [ | r | r := Array new: self size. 1 to: self size do: [:i | r at: i put: (aBlock value: (self at: i))]. ^r ]
    asByteArray [ ^self ]
    asString [ | s | s := WriteStream on: String new. self do: [:b | s nextPut: (Character value: b)]. ^s contents ]
    hex [ | s | s := WriteStream on: String new. self do: [:b | s nextPutAll: ('0123456789abcdef' at: b // 16 + 1) asString; nextPutAll: ('0123456789abcdef' at: b \\ 16 + 1) asString]. ^s contents ]
]
String extend [
    asByteArray [ | b | b := ByteArray new: self size. 1 to: self size do: [:i | b at: i put: (self at: i) value]. ^b ]
]
Array extend [
    asByteArray [ | b | b := ByteArray new: self size. 1 to: self size do: [:i | b at: i put: (self at: i)]. ^b ]
]

Magnitude subclass: Duration [
    | secs |
    Duration class >> seconds: n [ ^self basicNew setSeconds: n ]
    Duration class >> milliseconds: n [ ^self basicNew setSeconds: n / 1000 ]
    Duration class >> minutes: n [ ^self seconds: n * 60 ]
    Duration class >> hours: n [ ^self seconds: n * 3600 ]
    Duration class >> days: n [ ^self seconds: n * 86400 ]
    Duration class >> days: d hours: h minutes: m seconds: s [ ^self seconds: d * 86400 + (h * 3600) + (m * 60) + s ]
    Duration class >> zero [ ^self seconds: 0 ]
    setSeconds: n [ secs := n ]
    asSeconds [ ^secs ]
    asMilliseconds [ ^(secs * 1000) rounded ]
    negative [ ^secs < 0 ]
    abs [ ^Duration seconds: secs abs ]
    negated [ ^Duration seconds: secs negated ]
    days [ ^secs abs // 86400 * secs sign ]
    hours [ ^secs abs \\ 86400 // 3600 * secs sign ]
    minutes [ ^secs abs \\ 3600 // 60 * secs sign ]
    seconds [ ^secs abs \\ 60 * secs sign ]
    totalDays [ ^secs / 86400 ]
    + other [ ^Duration seconds: secs + other asSeconds ]
    - other [ ^Duration seconds: secs - other asSeconds ]
    * n [ ^Duration seconds: secs * n ]
    / x [ ^(x isKindOf: Duration) ifTrue: [secs / x asSeconds] ifFalse: [Duration seconds: secs / x] ]
    < other [ ^secs < other asSeconds ]
    = other [ ^(other isKindOf: Duration) and: [secs = other asSeconds] ]
    hash [ ^secs hash ]
    printOn: aStream [
        | a |
        a := secs abs.
        secs < 0 ifTrue: [aStream nextPut: $-].
        aStream print: a // 86400; nextPut: $:.
        aStream nextPutAll: ((a \\ 86400 // 3600) printPaddedWith: $0 to: 2).
        aStream nextPut: $:; nextPutAll: ((a \\ 3600 // 60) printPaddedWith: $0 to: 2).
        aStream nextPut: $:; nextPutAll: ((a \\ 60) truncated printPaddedWith: $0 to: 2)
    ]
]
Integer extend [
    seconds [ ^Duration seconds: self ]
    minutes [ ^Duration minutes: self ]
    hours [ ^Duration hours: self ]
    days [ ^Duration days: self ]
    milliseconds [ ^Duration milliseconds: self ]
]
DateTime extend [
    - other [
        ^(other isKindOf: DateTime)
            ifTrue: [Duration seconds: self asSeconds - other asSeconds]
            ifFalse: [self addSeconds: other asSeconds negated]
    ]
    + aDuration [ ^self addSeconds: aDuration asSeconds ]
    asDate [ ^date ]
    asTime [ ^time ]
]
Date extend [
    - other [
        ^(other isKindOf: Date)
            ifTrue: [Duration days: self asDays - other asDays]
            ifFalse: [self addDays: other asSeconds negated // 86400]
    ]
    + aDuration [ ^self addDays: aDuration asSeconds // 86400 ]
]

"dependents: changed:/update:"
Object extend [
    dependents [ ^(Smalltalk at: #Dependencies) at: self ifAbsent: [OrderedCollection new] ]
    addDependent: anObject [
        | d |
        d := (Smalltalk at: #Dependencies) at: self ifAbsent: [nil].
        d isNil ifTrue: [d := OrderedCollection new. (Smalltalk at: #Dependencies) at: self put: d].
        (d includes: anObject) ifFalse: [d add: anObject].
        ^anObject
    ]
    removeDependent: anObject [
        | d |
        d := (Smalltalk at: #Dependencies) at: self ifAbsent: [^anObject].
        d remove: anObject ifAbsent: [nil].
        d isEmpty ifTrue: [(Smalltalk at: #Dependencies) removeKey: self ifAbsent: [nil]].
        ^anObject
    ]
    release [ (Smalltalk at: #Dependencies) removeKey: self ifAbsent: [nil] ]
    changed [ self changed: self ]
    changed: aParameter [ self dependents do: [:d | d update: aParameter] ]
    update: aParameter [ ^self ]
    broadcast: aSymbol [ self dependents do: [:d | d perform: aSymbol] ]
    broadcast: aSymbol with: arg [ self dependents do: [:d | d perform: aSymbol with: arg] ]
]
Smalltalk at: #Dependencies put: IdentityDictionary new.

Object subclass: Rectangle [
    | origin corner |
    Rectangle class >> origin: o corner: c [ ^self basicNew setOrigin: o corner: c ]
    Rectangle class >> origin: o extent: e [ ^self basicNew setOrigin: o corner: o + e ]
    Rectangle class >> left: l right: r top: t bottom: b [ ^self origin: l @ t corner: r @ b ]
    setOrigin: o corner: c [ origin := o. corner := c ]
    origin [ ^origin ]
    corner [ ^corner ]
    extent [ ^corner - origin ]
    width [ ^corner x - origin x ]
    height [ ^corner y - origin y ]
    left [ ^origin x ]
    right [ ^corner x ]
    top [ ^origin y ]
    bottom [ ^corner y ]
    center [ ^(origin + corner) / 2 ]
    topLeft [ ^origin ]
    bottomRight [ ^corner ]
    area [ ^self width * self height ]
    containsPoint: p [ ^p x >= origin x and: [p x < corner x and: [p y >= origin y and: [p y < corner y]]] ]
    contains: r [ ^(self containsPoint: r origin) and: [r corner x <= corner x and: [r corner y <= corner y]] ]
    intersects: r [ ^origin x < r corner x and: [r origin x < corner x and: [origin y < r corner y and: [r origin y < corner y]]] ]
    intersect: r [ ^Rectangle origin: (origin x max: r origin x) @ (origin y max: r origin y) corner: (corner x min: r corner x) @ (corner y min: r corner y) ]
    merge: r [ ^Rectangle origin: (origin x min: r origin x) @ (origin y min: r origin y) corner: (corner x max: r corner x) @ (corner y max: r corner y) ]
    translateBy: p [ ^Rectangle origin: origin + p corner: corner + p ]
    expandBy: n [ ^Rectangle origin: origin - n corner: corner + n ]
    = r [ ^(r isKindOf: Rectangle) and: [origin = r origin and: [corner = r corner]] ]
    hash [ ^origin hash bitXor: corner hash ]
    printOn: aStream [ aStream print: origin; nextPutAll: ' corner: '; print: corner ]
]
Point extend [
    corner: p [ ^Rectangle origin: self corner: p ]
    extent: p [ ^Rectangle origin: self extent: p ]
]

Object subclass: CompiledMethod [
    | methodClass selector |
    CompiledMethod class >> class: c selector: s [ ^self basicNew setClass: c selector: s ]
    setClass: c selector: s [ methodClass := c. selector := s ]
    methodClass [ ^methodClass ]
    selector [ ^selector ]
    numArgs [ ^selector numArgs ]
    valueWithReceiver: r withArguments: args [ ^r perform: selector withArguments: args ]
    printOn: aStream [ aStream print: methodClass; nextPutAll: '>>'; nextPutAll: selector ]
]
Behavior extend [
    >> aSymbol [
        (self includesSelector: aSymbol) ifFalse: [^SystemExceptions.NotFound signalOn: aSymbol what: 'method' reason: 'method not found'].
        ^CompiledMethod class: self selector: aSymbol
    ]
    compiledMethodAt: aSymbol [ ^self >> aSymbol ]
    lookupSelector: aSymbol [
        | c |
        c := self.
        [c notNil] whileTrue: [(c includesSelector: aSymbol) ifTrue: [^c >> aSymbol]. c := c superclass].
        ^nil
    ]
    allInstances [ <primitive: allInstances> ^#() ]
    instanceCount [ ^self allInstances size ]
]

Object extend [
    become: other [ <primitive: become> ^self error: 'become: needs two heap objects' ]
]
