"kernel.st - Object, Boolean, nil, classes, Character, Association, Transcript
 Part of the CREXX Smalltalk class library."

Object extend [
    class [ <primitive: class> ]
    == other [ <primitive: ident> ]
    ~~ other [ <primitive: nident> ]
    = other [ ^self == other ]
    ~= other [ ^(self = other) == false ]
    hash [ <primitive: identityHash> ]
    identityHash [ <primitive: identityHash> ]
    isNil [ <primitive: isNil> ]
    notNil [ ^self isNil == false ]
    isString [ <primitive: isString> ]
    isSymbol [ <primitive: isSymbol> ]
    isCharacter [ <primitive: isCharacter> ]
    isNumber [ <primitive: isNumber> ]
    isInteger [ <primitive: isInteger> ]
    isFloat [ <primitive: isFloat> ]
    isFraction [ ^false ]
    isArray [ ^false ]
    isClass [ ^false ]
    isMetaclass [ ^false ]
    isCollection [ ^false ]
    isSequenceable [ ^false ]
    isDictionary [ ^false ]
    isBoolean [ ^false ]
    isKindOf: aClass [ <primitive: isKindOf> ^self error: 'argument must be a class' ]
    isMemberOf: aClass [ <primitive: isMemberOf> ^false ]
    respondsTo: aSymbol [ <primitive: respondsTo> ^false ]
    shallowCopy [ <primitive: shallowCopy> ]
    copy [ ^self shallowCopy postCopy ]
    postCopy [ ^self ]
    deepCopy [
        | c |
        c := self shallowCopy.
        1 to: self class instSize do: [:i | c instVarAt: i put: (self instVarAt: i) copy].
        ^c
    ]
    yourself [ ^self ]
    -> aValue [ ^Association key: self value: aValue ]
    species [ ^self class ]
    size [ <primitive: basicSize> ]
    basicSize [ <primitive: basicSize> ]
    basicAt: i [ <primitive: basicAt> ^self checkIndex: i ]
    basicAt: i put: v [ <primitive: basicAtPut> ^self checkIndex: i ]
    at: i [ <primitive: basicAt> ^self checkIndex: i ]
    at: i put: v [ <primitive: basicAtPut> ^self checkIndex: i ]
    checkIndex: i [ ^SystemExceptions.IndexOutOfRange signalOn: self withIndex: i ]
    instVarAt: i [ <primitive: instVarAt> ^self checkIndex: i ]
    instVarAt: i put: v [ <primitive: instVarAtPut> ^self checkIndex: i ]
    perform: s [ <primitive: perform> ^self primPerformFailed: s ]
    perform: s with: a [ <primitive: perform> ^self primPerformFailed: s ]
    perform: s with: a with: b [ <primitive: perform> ^self primPerformFailed: s ]
    perform: s with: a with: b with: c [ <primitive: perform> ^self primPerformFailed: s ]
    perform: s with: a with: b with: c with: d [ <primitive: perform> ^self primPerformFailed: s ]
    perform: s withArguments: args [ <primitive: performWithArgs> ^self primPerformFailed: s ]
    primPerformFailed: s [ ^SystemExceptions.WrongClass signalOn: s reason: 'invalid selector' ]
    ifNil: aBlock [ ^self ]
    ifNotNil: aBlock [ ^aBlock numArgs = 0 ifTrue: [aBlock value] ifFalse: [aBlock value: self] ]
    ifNil: nilBlock ifNotNil: aBlock [ ^aBlock numArgs = 0 ifTrue: [aBlock value] ifFalse: [aBlock value: self] ]
    ifNotNil: aBlock ifNil: nilBlock [ ^aBlock numArgs = 0 ifTrue: [aBlock value] ifFalse: [aBlock value: self] ]
    isNil: x [ ^false ]

    printOn: aStream [
        aStream nextPutAll: self class article; space; nextPutAll: self class name
    ]
    printString [
        | s |
        s := WriteStream on: String new.
        self printOn: s.
        ^s contents
    ]
    displayString [ ^self printString ]
    displayNl [ Transcript showCr: self displayString ]
    printNl [ Transcript showCr: self printString ]
    display [ Transcript show: self displayString ]
    print [ Transcript show: self printString ]
    storeString [ ^self printString ]
    printString: base [ ^self printString ]
    inspect [ self printNl ]

    error: message [ ^Error new receiver: self; signal: message ]
    doesNotUnderstand: aMessage [
        ^MessageNotUnderstood new message: aMessage; receiver: self; signal
    ]
    subclassResponsibility [ ^self error: 'This method is a subclass responsibility' ]
    halt [ ^self error: 'halt' ]
    halt: msg [ ^self error: msg ]
    assert: aBoolean [ aBoolean ifFalse: [^self error: 'Assertion failed']. ^self ]
    shouldNotImplement [ ^self error: 'This method should not be called for instances of this class' ]
    notYetImplemented [ ^self error: 'not yet implemented' ]
    value [ ^self ]
]

Behavior extend [
    new [ <primitive: basicNew> ^self error: 'cannot create instances of this class' ]
    new: n [ <primitive: basicNewSize> ^self error: 'cannot create instances of this class' ]
    basicNew [ <primitive: basicNew> ^self error: 'cannot create instances of this class' ]
    basicNew: n [ <primitive: basicNewSize> ^self error: 'cannot create instances of this class' ]
    name [ <primitive: className> ]
    printString [ ^self name ]
    displayString [ ^self name ]
    storeString [ ^self name ]
    printOn: aStream [ aStream nextPutAll: self name ]
    article [
        | n |
        n := self name.
        ^(n size > 0 and: ['AEIOU' includes: (n at: 1)]) ifTrue: ['an'] ifFalse: ['a']
    ]
    superclass [ <primitive: superclass> ]
    subclasses [ <primitive: subclasses> ]
    allSubclasses [
        | r |
        r := OrderedCollection new.
        self subclasses do: [:c | r add: c. r addAll: c allSubclasses].
        ^r
    ]
    allSuperclasses [
        | r c |
        r := OrderedCollection new.
        c := self superclass.
        [c isNil] whileFalse: [r add: c. c := c superclass].
        ^r
    ]
    withAllSuperclasses [ ^(OrderedCollection with: self) addAll: self allSuperclasses; yourself ]
    selectors [ <primitive: selectors> ]
    includesSelector: aSymbol [ <primitive: includesSelector> ^false ]
    canUnderstand: aSymbol [ <primitive: lookupSelector> ^false ]
    inheritsFrom: aClass [ ^self ~~ aClass and: [self allSuperclasses includes: aClass] ]
    includesBehavior: aClass [ ^self == aClass or: [self inheritsFrom: aClass] ]
    primIvars [ <primitive: instanceVariableNames> ]
    allInstVarNames [ ^self primIvars subStrings collect: [:s | s asSymbol] ]
    instVarNames [
        | all sup |
        all := self allInstVarNames.
        sup := self superclass.
        sup isNil ifTrue: [^all].
        ^all copyFrom: sup instSize + 1 to: all size
    ]
    instanceVariableNames [ ^self instVarNames ]
    instSize [ ^self allInstVarNames size ]
    instanceVariableNames: aString [
        aString subStrings do: [:v | self addInstVarName: v]
    ]
    addInstVarName: aString [ <primitive: addInstVar> ]
    addClassVarName: aString [ <primitive: addClassVar> ]
    category [ ^nil ]
    category: aString [ ^self ]
    comment [ ^nil ]
    comment: aString [ ^self ]
    isClass [ ^true ]
    class [ <primitive: class> ]
    subclass: aSymbol [ <primitive: subclassNamed> ]
    subclass: aSymbol instanceVariableNames: iv classVariableNames: cv package: p [
        | c |
        c := self subclass: aSymbol.
        c instanceVariableNames: iv.
        cv subStrings do: [:v | c addClassVarName: v].
        ^c
    ]
    subclass: aSymbol instanceVariableNames: iv classVariableNames: cv category: p [
        ^self subclass: aSymbol instanceVariableNames: iv classVariableNames: cv package: p
    ]
    subclass: aSymbol instanceVariableNames: iv classVariableNames: cv poolDictionaries: pd category: p [
        ^self subclass: aSymbol instanceVariableNames: iv classVariableNames: cv package: p
    ]
    subclass: aSymbol instanceVariableNames: iv classVariableNames: cv poolDictionaries: pd [
        ^self subclass: aSymbol instanceVariableNames: iv classVariableNames: cv package: nil
    ]
]

Metaclass extend [
    isMetaclass [ ^true ]
    isClass [ ^false ]
    instanceClass [ ^nil ]
]

UndefinedObject extend [
    isNil [ ^true ]
    notNil [ ^false ]
    ifNil: aBlock [ ^aBlock value ]
    ifNotNil: aBlock [ ^nil ]
    ifNil: nilBlock ifNotNil: aBlock [ ^nilBlock value ]
    ifNotNil: aBlock ifNil: nilBlock [ ^nilBlock value ]
    printString [ ^'nil' ]
    displayString [ ^'nil' ]
    printOn: aStream [ aStream nextPutAll: 'nil' ]
    isEmpty [ ^true ]
    copy [ ^nil ]
    deepCopy [ ^nil ]
    shallowCopy [ ^nil ]
]

Boolean extend [
    isBoolean [ ^true ]
    copy [ ^self ]
    deepCopy [ ^self ]
    shallowCopy [ ^self ]
    displayString [ ^self printString ]
    asString [ ^self printString ]
    printOn: aStream [ aStream nextPutAll: self printString ]
]

True extend [
    printString [ ^'true' ]
    not [ ^false ]
    & aBoolean [ ^aBoolean ]
    | aBoolean [ ^true ]
    and: aBlock [ ^aBlock value ]
    or: aBlock [ ^true ]
    xor: aBoolean [ ^aBoolean not ]
    eqv: aBoolean [ ^aBoolean ]
    ifTrue: aBlock [ ^aBlock value ]
    ifFalse: aBlock [ ^nil ]
    ifTrue: tBlock ifFalse: fBlock [ ^tBlock value ]
    ifFalse: fBlock ifTrue: tBlock [ ^tBlock value ]
]

False extend [
    printString [ ^'false' ]
    not [ ^true ]
    & aBoolean [ ^false ]
    | aBoolean [ ^aBoolean ]
    and: aBlock [ ^false ]
    or: aBlock [ ^aBlock value ]
    xor: aBoolean [ ^aBoolean ]
    eqv: aBoolean [ ^aBoolean not ]
    ifTrue: aBlock [ ^nil ]
    ifFalse: aBlock [ ^aBlock value ]
    ifTrue: tBlock ifFalse: fBlock [ ^fBlock value ]
    ifFalse: fBlock ifTrue: tBlock [ ^fBlock value ]
]

Magnitude extend [
    < aMagnitude [ ^self subclassResponsibility ]
    > aMagnitude [ ^aMagnitude < self ]
    <= aMagnitude [ ^(aMagnitude < self) not ]
    >= aMagnitude [ ^(self < aMagnitude) not ]
    between: min and: max [ ^self >= min and: [self <= max] ]
    min: aMagnitude [ ^self < aMagnitude ifTrue: [self] ifFalse: [aMagnitude] ]
    max: aMagnitude [ ^self > aMagnitude ifTrue: [self] ifFalse: [aMagnitude] ]
]

Character class extend [
    value: n [ <primitive: charFrom> ^SystemExceptions.ArgumentOutOfRange signalOn: n mustBeBetween: 0 and: 255 ]
    codePoint: n [ ^self value: n ]
    digitValue: n [ ^'0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ' at: n + 1 ]
    nl [ ^self value: 10 ]
    lf [ ^self value: 10 ]
    cr [ ^self value: 13 ]
    tab [ ^self value: 9 ]
    space [ ^self value: 32 ]
    esc [ ^self value: 27 ]
    eof [ ^self value: 26 ]
    nul [ ^self value: 0 ]
    new [ ^self shouldNotImplement ]
]

Character extend [
    value [ <primitive: charValue> ]
    asInteger [ <primitive: charValue> ]
    codePoint [ <primitive: charValue> ]
    asCharacter [ ^self ]
    < aChar [ ^self value < aChar value ]
    > aChar [ ^self value > aChar value ]
    <= aChar [ ^self value <= aChar value ]
    >= aChar [ ^self value >= aChar value ]
    isLetter [ | v | v := self value. ^(v >= 65 and: [v <= 90]) or: [v >= 97 and: [v <= 122]] ]
    isDigit [ | v | v := self value. ^v >= 48 and: [v <= 57] ]
    isAlphaNumeric [ ^self isLetter or: [self isDigit] ]
    isUppercase [ | v | v := self value. ^v >= 65 and: [v <= 90] ]
    isLowercase [ | v | v := self value. ^v >= 97 and: [v <= 122] ]
    isSeparator [ | v | v := self value. ^v = 32 or: [v = 9 or: [v = 10 or: [v = 13 or: [v = 12]]]] ]
    isSpace [ ^self isSeparator ]
    isPunctuation [ ^'.,:;!?' includes: self ]
    isVowel [ ^'aeiouAEIOU' includes: self ]
    isCharacter [ ^true ]
    asUppercase [ ^self isLowercase ifTrue: [Character value: self value - 32] ifFalse: [self] ]
    asLowercase [ ^self isUppercase ifTrue: [Character value: self value + 32] ifFalse: [self] ]
    digitValue [
        | v |
        v := self value.
        (v >= 48 and: [v <= 57]) ifTrue: [^v - 48].
        (v >= 65 and: [v <= 90]) ifTrue: [^v - 55].
        (v >= 97 and: [v <= 122]) ifTrue: [^v - 87].
        ^self error: 'Not a digit character'
    ]
    asString [ ^String with: self ]
    asSymbol [ ^(String with: self) asSymbol ]
    displayString [ ^String with: self ]
    printString [
        | v |
        v := self value.
        (v > 32 and: [v < 127]) ifTrue: [^'$', (String with: self)].
        v = 32 ifTrue: [^'$ '].
        ^'Character value: ', v printString
    ]
    printOn: aStream [ aStream nextPutAll: self printString ]
    storeString [ ^self printString ]
    copy [ ^self ]
    shallowCopy [ ^self ]
    deepCopy [ ^self ]
    * n [ ^String new: n withAll: self ]
]

LookupKey extend [
    | key |
    key [ ^key ]
    key: k [ key := k ]
    < other [ ^key < other key ]
]

LookupKey class extend [ key: k [ ^self new key: k ] ]

Association extend [
    | value |
    value [ ^value ]
    value: v [ value := v ]
    key: k value: v [ key := k. value := v ]
    = other [ ^other class == self class and: [key = other key and: [value = other value]] ]
    hash [ ^key hash bitXor: value hash ]
    printOn: aStream [ aStream print: key; nextPutAll: '->'; print: value ]
    displayString [ ^key displayString, '->', value displayString ]
]

Association class extend [ key: k value: v [ ^self new key: k value: v ] ]

Message extend [
    | selector args |
    selector [ ^selector ]
    arguments [ ^args ]
    selector: s arguments: a [ selector := s. args := a ]
    printOn: aStream [ aStream nextPutAll: selector ]
    reinvokeFor: rcv [ ^rcv perform: selector withArguments: args ]
]

Message class extend [ selector: s arguments: a [ ^self new selector: s arguments: a ] ]

TextCollector class extend [ new [ ^self basicNew ] ]

TextCollector extend [
    nextPutAll: s [ <primitive: transcript> ^self nextPutAll: s displayString ]
    nextPut: c [ ^self nextPutAll: (String with: c) ]
    show: s [ ^self nextPutAll: s ]
    showCr: s [ self nextPutAll: s. self nl ]
    display: x [ ^self nextPutAll: x displayString ]
    print: x [ ^self nextPutAll: x printString ]
    displayNl: x [ self nextPutAll: x displayString. self nl ]
    printNl: x [ self nextPutAll: x printString. self nl ]
    << x [ ^self nextPutAll: x displayString ]
    nl [ ^self nextPutAll: '
' ]
    cr [ ^self nl ]
    newLine [ ^self nl ]
    tab [ ^self nextPutAll: '	' ]
    space [ ^self nextPutAll: ' ' ]
    tab: n [ n timesRepeat: [self tab] ]
    space: n [ n timesRepeat: [self space] ]
    nl: n [ n timesRepeat: [self nl] ]
    flush [ ^self ]
    ensureCr [ ^self ]
]

SystemDictionary extend [
    at: key [ <primitive: globalAt> ^SystemExceptions.NotFound signalOn: key what: 'key' reason: 'key not found' ]
    at: key put: value [ <primitive: globalAtPut> ]
    at: key ifAbsent: aBlock [ ^(self includesKey: key) ifTrue: [self at: key] ifFalse: [aBlock value] ]
    includesKey: key [ <primitive: globalHas> ]
    includes: key [ ^self includesKey: key ]
    version [ ^'CREXX Smalltalk 1.0' ]
    readLine [ <primitive: stdinLine> ]
    stdinAtEnd [ <primitive: stdinAtEnd> ]
    halt [ <primitive: halt> ]
    quit [ <primitive: exit> ]
    quit: n [ <primitive: exit> ]
    classAt: key [ ^self at: key ]
    printOn: aStream [ aStream nextPutAll: 'Smalltalk' ]
]

Time class extend [
    millisecondClock [ <primitive: msecs> ]
    millisecondClockValue [ <primitive: msecs> ]
    millisecondsToRun: aBlock [ | t | t := self millisecondClock. aBlock value. ^self millisecondClock - t ]
]

Random class extend [
    seed: n [ ^self new ]
    next [ ^self new next ]
    between: lo and: hi [ ^self new between: lo and: hi ]
]
Random extend [
    primRandom: n [ <primitive: random> ]
    next [ ^self primRandom: nil ]
    between: lo and: hi [ ^lo + (self primRandom: hi - lo + 1) - 1 ]
    nextInt: n [ ^self primRandom: n ]
]

Point class extend [ x: ax y: ay [ ^self new x: ax y: ay ] ]
Point extend [
    | x y |
    x [ ^x ]
    y [ ^y ]
    x: ax y: ay [ x := ax. y := ay ]
    x: ax [ x := ax ]
    y: ay [ y := ay ]
    + p [ ^p isNumber ifTrue: [Point x: x + p y: y + p] ifFalse: [Point x: x + p x y: y + p y] ]
    - p [ ^p isNumber ifTrue: [Point x: x - p y: y - p] ifFalse: [Point x: x - p x y: y - p y] ]
    * p [ ^p isNumber ifTrue: [Point x: x * p y: y * p] ifFalse: [Point x: x * p x y: y * p y] ]
    / p [ ^p isNumber ifTrue: [Point x: x / p y: y / p] ifFalse: [Point x: x / p x y: y / p y] ]
    = p [ ^(p isKindOf: Point) and: [x = p x and: [y = p y]] ]
    hash [ ^x hash bitXor: y hash ]
    < p [ ^x < p x and: [y < p y] ]
    dist: p [ ^((x - p x) squared + (y - p y) squared) sqrt ]
    printOn: aStream [ aStream print: x; nextPut: $@; print: y ]
]

Date class extend [
    primString [ <primitive: dateStr> ]
    today [
        | s |
        s := self primString.
        ^self new setYear: (s copyFrom: 1 to: 4) asNumber month: (s copyFrom: 5 to: 6) asNumber day: (s copyFrom: 7 to: 8) asNumber
    ]
    year: y month: m day: d [ ^self new setYear: y month: m day: d ]
    fromDays: n [
        | z era doe yoe y doy mp d m |
        z := n + 719468.
        era := z // 146097.
        doe := z - (era * 146097).
        yoe := (doe - (doe // 1460) + (doe // 36524) - (doe // 146096)) // 365.
        y := yoe + (era * 400).
        doy := doe - ((365 * yoe) + (yoe // 4) - (yoe // 100)).
        mp := ((5 * doy) + 2) // 153.
        d := doy - (((153 * mp) + 2) // 5) + 1.
        m := mp < 10 ifTrue: [mp + 3] ifFalse: [mp - 9].
        m <= 2 ifTrue: [y := y + 1].
        ^self year: y month: m day: d
    ]
    isLeapYear: y [ ^(y \\ 4 = 0 and: [y \\ 100 ~= 0]) or: [y \\ 400 = 0] ]
    daysInMonth: m year: y [
        m = 2 ifTrue: [^(self isLeapYear: y) ifTrue: [29] ifFalse: [28]].
        ^(#(31 28 31 30 31 30 31 31 30 31 30 31) at: m)
    ]
]
Date extend [
    | year month day |
    asDays [
        | y era yoe doy doe |
        y := month <= 2 ifTrue: [year - 1] ifFalse: [year].
        era := y // 400.
        yoe := y - (era * 400).
        doy := (((153 * (month > 2 ifTrue: [month - 3] ifFalse: [month + 9])) + 2) // 5) + day - 1.
        doe := (yoe * 365) + (yoe // 4) - (yoe // 100) + doy.
        ^(era * 146097) + doe - 719468
    ]
    addDays: n [ ^Date fromDays: self asDays + n ]
    subtractDate: other [ ^self asDays - other asDays ]
    daysFrom: other [ ^self asDays - other asDays ]
    dayOfWeek [ ^((self asDays + 3) \\ 7) + 1 ]
    dayName [ ^#(#Monday #Tuesday #Wednesday #Thursday #Friday #Saturday #Sunday) at: self dayOfWeek ]
    monthName [ ^#(#January #February #March #April #May #June #July #August #September #October #November #December) at: month ]
    isLeapYear [ ^Date isLeapYear: year ]
    daysInMonth [ ^Date daysInMonth: month year: year ]

    setYear: y month: m day: d [ year := y. month := m. day := d ]
    year [ ^year ]
    month [ ^month ]
    day [ ^day ]
    dayOfMonth [ ^day ]
    < other [ ^self asNumber < other asNumber ]
    = other [ ^(other isKindOf: Date) and: [self asNumber = other asNumber] ]
    hash [ ^self asNumber ]
    asNumber [ ^year * 10000 + (month * 100) + day ]
    printOn: aStream [
        aStream nextPutAll: (year printPaddedWith: $0 to: 4); nextPut: $-;
            nextPutAll: (month printPaddedWith: $0 to: 2); nextPut: $-;
            nextPutAll: (day printPaddedWith: $0 to: 2)
    ]
]
Time class extend [
    primString [ <primitive: timeStr> ]
    now [
        | s |
        s := self primString.
        ^self new setHours: (s copyFrom: 1 to: 2) asNumber minutes: (s copyFrom: 4 to: 5) asNumber seconds: (s copyFrom: 7 to: 8) asNumber
    ]
    hours: h minutes: m seconds: s [ ^self new setHours: h minutes: m seconds: s ]
    fromSeconds: n [ | t | t := n \\ 86400. ^self hours: t // 3600 minutes: t \\ 3600 // 60 seconds: t \\ 60 ]
]
Time extend [
    | hours minutes seconds |
    setHours: h minutes: m seconds: s [ hours := h. minutes := m. seconds := s ]
    hours [ ^hours ]
    minutes [ ^minutes ]
    seconds [ ^seconds ]
    asSeconds [ ^hours * 3600 + (minutes * 60) + seconds ]
    addSeconds: n [ ^Time fromSeconds: self asSeconds + n ]
    subtractTime: other [ ^self asSeconds - other asSeconds ]
    < other [ ^self asSeconds < other asSeconds ]
    = other [ ^(other isKindOf: Time) and: [self asSeconds = other asSeconds] ]
    hash [ ^self asSeconds ]
    printOn: aStream [
        aStream nextPutAll: (hours printPaddedWith: $0 to: 2); nextPut: $:;
            nextPutAll: (minutes printPaddedWith: $0 to: 2); nextPut: $:;
            nextPutAll: (seconds printPaddedWith: $0 to: 2)
    ]
]

Date extend [
    longPrintString [
        ^self dayName asString, ', ', day printString, ' ', self monthName asString, ' ', year printString
    ]
    shortMonthName [ ^self monthName asString copyFrom: 1 to: 3 ]
    dayOfYear [ ^self asDays - (Date year: year month: 1 day: 1) asDays + 1 ]
    isWeekend [ ^self dayOfWeek > 5 ]
    addMonths: n [
        | m y d |
        m := month - 1 + n.
        y := year + (m // 12).
        m := m \\ 12 + 1.
        d := day min: (Date daysInMonth: m year: y).
        ^Date year: y month: m day: d
    ]
    asDate [ ^self ]
]

Magnitude subclass: DateTime [
    | date time |
    DateTime class >> now [ ^self date: Date today time: Time now ]
    DateTime class >> date: d time: t [ ^self new setDate: d time: t ]
    DateTime class >> year: y month: m day: d hour: h minute: mi second: s [
        ^self date: (Date year: y month: m day: d) time: (Time hours: h minutes: mi seconds: s)
    ]
    DateTime class >> fromSeconds: n [
        ^self date: (Date fromDays: n // 86400) time: (Time fromSeconds: n \\ 86400)
    ]
    setDate: d time: t [ date := d. time := t ]
    date [ ^date ]
    time [ ^time ]
    year [ ^date year ]
    month [ ^date month ]
    day [ ^date day ]
    hours [ ^time hours ]
    minutes [ ^time minutes ]
    seconds [ ^time seconds ]
    asSeconds [ ^date asDays * 86400 + time asSeconds ]
    addSeconds: n [ ^DateTime fromSeconds: self asSeconds + n ]
    addDays: n [ ^DateTime date: (date addDays: n) time: time ]
    subtractDateTime: other [ ^self asSeconds - other asSeconds ]
    < other [ ^self asSeconds < other asSeconds ]
    = other [ ^(other isKindOf: DateTime) and: [self asSeconds = other asSeconds] ]
    hash [ ^self asSeconds ]
    printOn: aStream [
        aStream print: date; nextPut: $T; print: time
    ]
]
