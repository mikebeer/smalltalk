"more2.st - protocol that programs written for Pharo / GNU Smalltalk often use"

"---- Object ----"
Object extend [
    flag: aSymbol [ ^self ]
    deprecated: aString [ ^self ]
    ifNotNilDo: aBlock [ ^self isNil ifTrue: [nil] ifFalse: [aBlock value: self] ]
    tap: aBlock [ aBlock value: self ]
    isInterval [ ^false ]
    isStream [ ^false ]
    isSet [ ^false ]
    isPoint [ ^false ]
    isRectangle [ ^false ]
    isCompiledMethod [ ^false ]
    isUndefined [ ^false ]
    isClosure [ ^false ]
    isSmallInteger [ ^false ]
    isBehavior [ ^false ]
    pointsTo: anObject [
        1 to: self class instSize do: [:i | (self instVarAt: i) == anObject ifTrue: [^true]].
        ^false
    ]
    printStringLimitedTo: n [
        | s |
        s := self printString.
        ^s size <= n ifTrue: [s] ifFalse: [(s copyFrom: 1 to: n), '...']
    ]
    displayStringLimitedTo: n [
        | s |
        s := self displayString.
        ^s size <= n ifTrue: [s] ifFalse: [(s copyFrom: 1 to: n), '...']
    ]
    storeOn: aStream [ aStream nextPutAll: self storeString ]
]
UndefinedObject extend [ isUndefined [ ^true ] ]
BlockClosure extend [ isClosure [ ^true ] ]
Interval extend [ isInterval [ ^true ] ]
Set extend [ isSet [ ^true ] ]
Point extend [ isPoint [ ^true ] ]
Rectangle extend [ isRectangle [ ^true ] ]
CompiledMethod extend [ isCompiledMethod [ ^true ] ]
SmallInteger extend [ isSmallInteger [ ^true ] ]
Behavior extend [
    isBehavior [ ^true ]
    instanceSide [ ^self ]
    theNonMetaClass [ ^self ]
    isFixed [ ^true ]
    hasMethods [ ^self selectors notEmpty ]
    allSubclassesDo: aBlock [ self allSubclasses do: aBlock ]
    allSuperclassesDo: aBlock [ self allSuperclasses do: aBlock ]
    selectorsAndMethodsDo: aBlock [ self selectors do: [:s | aBlock value: s value: self >> s] ]
    whichClassIncludesSelector: aSymbol [
        | c |
        c := self.
        [c isNil] whileFalse: [
            (c includesSelector: aSymbol) ifTrue: [^c].
            c := c superclass].
        ^nil
    ]
]

"---- String ----"
String extend [
    beginsWith: aString [ ^self startsWith: aString ]
    withBlanksTrimmed [ ^self trimSeparators ]
    trimBoth [ ^self trimSeparators ]
    lineCount [ ^self lines size ]
    findString: aString [ ^self indexOf: aString ]
    contractTo: n [
        ^self size <= n ifTrue: [self] ifFalse: [(self copyFrom: 1 to: n - 3), '...']
    ]
    levenshtein: other [
        | prev cur a b cost |
        a := self. b := other.
        prev := (0 to: b size) asArray.
        1 to: a size do: [:i |
            cur := Array new: b size + 1.
            cur at: 1 put: i.
            1 to: b size do: [:j |
                cost := (a at: i) = (b at: j) ifTrue: [0] ifFalse: [1].
                cur at: j + 1 put: (((prev at: j + 1) + 1) min: (((cur at: j) + 1) min: ((prev at: j) + cost)))].
            prev := cur].
        ^prev at: b size + 1
    ]
]
String class extend [
    crlf [ ^String with: Character cr with: Character lf ]
    space [ ^String with: Character space ]
    empty [ ^String new ]
    newFrom: aCollection [ ^(aCollection inject: WriteStream on: String new into: [:ws :c | ws nextPut: c. ws]) contents ]
    fromCharacters: aCollection [ ^self newFrom: aCollection ]
    streamContents: aBlock [
        | ws |
        ws := WriteStream on: String new.
        aBlock value: ws.
        ^ws contents
    ]
    new: n streamContents: aBlock [ ^self streamContents: aBlock ]
    join: aCollection with: aString [ ^aCollection inject: '' into: [:acc :e | acc isEmpty ifTrue: [e displayString] ifFalse: [acc, aString, e displayString]] ]
]
Symbol extend [
    keywords [ ^(self asString subStrings: ':') collect: [:k | k, ':'] ]
    isKeyword [ ^self notEmpty and: [self last = $:] ]
    isUnary [ ^self notEmpty and: [self first isLetter and: [self isKeyword not]] ]
    isBinary [ ^self notEmpty and: [self first isLetter not] ]
    isInfix [ ^self isBinary ]
]
Character class extend [
    null [ ^self value: 0 ]
    newPage [ ^self value: 12 ]
    backspace [ ^self value: 8 ]
    escape [ ^self value: 27 ]
    delete [ ^self value: 127 ]
    bell [ ^self value: 7 ]
    alphabet [ ^'abcdefghijklmnopqrstuvwxyz' ]
]
Character extend [
    to: aCharacter [ ^(self value to: aCharacter value) collect: [:v | Character value: v] ]
    isSpecial [ ^self isLetter not and: [self isDigit not and: [self isSeparator not]] ]
]

"---- numbers ----"
Integer extend [
    asFraction [ ^self ]
    bitClear: mask [ ^self - (self bitAnd: mask) ]
    isPerfectSquare [ ^self >= 0 and: [self sqrtFloor squared = self] ]
    nthRoot: n [
        | r |
        r := (self asFloat raisedTo: 1 / n asFloat) rounded.
        [(r raisedTo: n) > self] whileTrue: [r := r - 1].
        [((r + 1) raisedTo: n) <= self] whileTrue: [r := r + 1].
        ^r
    ]
    log10 [ ^self asFloat log10 ]
    atRandom [ ^Random between: 1 and: self ]
    asLargerInteger [ ^self ]
]
Number extend [
    atRandom [ ^Random between: 1 and: self truncated ]
]
Float class extend [
    halfPi [ ^self pi / 2 ]
    twoPi [ ^self pi * 2 ]
    ln2 [ ^2 ln ]
    ln10 [ ^10 ln ]
    sqrt2 [ ^2 sqrt ]
    phi [ ^(1 + 5 sqrt) / 2 ]
    goldenRatio [ ^self phi ]
    zero [ ^0.0 ]
    one [ ^1.0 ]
]
Float extend [
    log10 [ ^self log: 10 ]
    floorLog: radix [ ^(self log: radix) floor ]
    roundUpTo: aNumber [ ^(self / aNumber) ceiling * aNumber ]
    printDecimalPlaces: n [ ^self printShowingDecimalPlaces: n ]
    isFinite [ ^self isNaN not and: [self isInfinite not] ]
    exponent [ ^self = 0.0 ifTrue: [0] ifFalse: [(self abs log: 2) floor] ]
    timesTwoPower: n [ ^self * (2 raisedTo: n) ]
    asPoint [ ^Point x: self y: self ]
]
Number extend [
    asPoint [ ^Point x: self y: self ]
]
Fraction extend [ reduced [ ^self ] ]

"---- Random ----"
Random extend [
    nextFloat [ ^self next ]
    nextBoolean [ ^(self between: 0 and: 1) = 1 ]
    next: n [ ^(1 to: n) collect: [:i | self next] ]
    seed: n [ ^Random seed: n ]
    shuffle: aCollection [
        | a j t |
        a := aCollection asArray copy.
        a size to: 2 by: -1 do: [:i |
            j := self between: 1 and: i.
            t := a at: i. a at: i put: (a at: j). a at: j put: t].
        ^a
    ]
    chooseFrom: aCollection [ ^aCollection asArray at: (self between: 1 and: aCollection size) ]
    random [ ^self next ]
]

"---- collections ----"
SequenceableCollection extend [
    atRandom [ ^self at: (Random between: 1 and: self size) ]
    atRandom: aRandom [ ^self at: (aRandom between: 1 and: self size) ]
    indexOfSubCollection: sub [
        1 to: self size - sub size + 1 do: [:i |
            (self copyFrom: i to: i + sub size - 1) = sub ifTrue: [^i]].
        ^0
    ]
    binarySearch: target [
        | lo hi mid v |
        lo := 1. hi := self size.
        [lo <= hi] whileTrue: [
            mid := lo + hi // 2.
            v := self at: mid.
            v = target ifTrue: [^mid].
            v < target ifTrue: [lo := mid + 1] ifFalse: [hi := mid - 1]].
        ^0
    ]
    allOccurrencesOf: x [ ^self select: [:e | e = x] ]
    transposed [ ^(1 to: self first size) collect: [:i | self collect: [:row | row at: i]] ]
    sumNumbers [ ^self inject: 0 into: [:a :b | a + b] ]
]
Array extend [
    replaceAll: old with: new [ 1 to: self size do: [:i | (self at: i) = old ifTrue: [self at: i put: new]] ]
]
Array class extend [
    streamContents: aBlock [
        | ws |
        ws := WriteStream on: Array new.
        aBlock value: ws.
        ^ws contents
    ]
    newFrom: aCollection [ ^aCollection asArray ]
    empty [ ^Array new: 0 ]
]
OrderedCollection extend [
    removeFirstOrNil [ ^self isEmpty ifTrue: [nil] ifFalse: [self removeFirst] ]
    removeLastOrNil [ ^self isEmpty ifTrue: [nil] ifFalse: [self removeLast] ]
    addAllLast: aCollection [ ^self addAll: aCollection ]
]
OrderedCollection class extend [
    newFrom: aCollection [ ^self withAll: aCollection ]
    empty [ ^self new ]
]
Set class extend [ newFrom: aCollection [ ^self withAll: aCollection ] ]
Dictionary extend [
    valuesAsArray [ ^self values asArray ]
    at: key update: aBlock [ ^self at: key put: (aBlock value: (self at: key)) ]
]
Dictionary class extend [
    newFrom: aCollection [
        | d |
        d := self new.
        aCollection do: [:a | d at: a key put: a value].
        ^d
    ]
    keys: keys values: values [
        | d |
        d := self new.
        keys with: values do: [:k :v | d at: k put: v].
        ^d
    ]
    empty [ ^self new ]
]
SortedCollection extend [
    indexForInserting: x [
        | i |
        i := 1.
        [i <= self size and: [self sortBlock value: (self at: i) value: x]] whileTrue: [i := i + 1].
        ^i
    ]
]
Interval extend [ reverseContents [ ^self asArray reverse ] ]

"---- streams ----"
Stream extend [
    close [ ^self ]
    flush [ ^self ]
    nextAvailable: n [
        | r |
        r := OrderedCollection new.
        [r size < n and: [self atEnd not]] whileTrue: [r add: self next].
        ^r asArray
    ]
    upToAll: pattern [
        | out n |
        out := OrderedCollection new.
        n := pattern size.
        [self atEnd] whileFalse: [
            out add: self next.
            (out size >= n and: [(out copyFrom: out size - n + 1 to: out size) asArray = pattern asArray]) ifTrue: [
                ^self primCollected: (out copyFrom: 1 to: out size - n)]].
        ^self primCollected: out
    ]
    primCollected: anOrderedCollection [
        ^(self respondsTo: #species) ifTrue: [anOrderedCollection asArray] ifFalse: [anOrderedCollection asArray]
    ]
]
ReadStream extend [
    primCollected: oc [
        | c |
        c := self contents.
        ^c isString ifTrue: [(oc inject: WriteStream on: String new into: [:ws :ch | ws nextPut: ch. ws]) contents] ifFalse: [oc asArray]
    ]
]
WriteStream extend [
    nextPutAll: aCollection startingAt: start [ ^self nextPutAll: (aCollection copyFrom: start to: aCollection size) ]
    truncate [ ^self reset ]
]
TextCollector extend [
    crShow: aString [ self cr; show: aString ]
    clear [ ^self ]
    endEntry [ ^self ]
    redirectToStdout [ ^self stopRedirect ]
]
BlockClosure extend [
    cull: a cull: b [ ^self numArgs = 0 ifTrue: [self value] ifFalse: [self numArgs = 1 ifTrue: [self value: a] ifFalse: [self value: a value: b]] ]
    durationToRun [ | t | t := Time millisecondClock. self value. ^Duration milliseconds: Time millisecondClock - t ]
    bench [ ^self durationToRun ]
]
Error extend [
    resignalAs: anException [ ^self return: anException signal ]
    tag [ ^nil ]
    signalerContext [ ^nil ]
]
Semaphore class extend [ forMutualExclusion [ ^self new signal; yourself ] ]

"---- Smalltalk ----"
SystemDictionary extend [
    garbageCollect [ ^self ]
    allClasses [ ^Object allSubclasses asArray , {Object} ]
    classes [ ^self allClasses ]
    classNamed: aString [ ^self at: aString asSymbol ifAbsent: [nil] ]
    hasClassNamed: aString [ ^(self classNamed: aString) isClass ]
    at: key ifPresent: aBlock [ (self includesKey: key) ifTrue: [^aBlock value: (self at: key)]. ^nil ]
    isHeadless [ ^true ]
    platformName [ ^self os ]
    hostSystem [ ^self os ]
    millisecondClock [ ^Time millisecondClock ]
    vmVersion [ ^self version ]
]

"---- Point ----"
Point extend [
    dotProduct: p [ ^(x * p x) + (y * p y) ]
    crossProduct: p [ ^(x * p y) - (y * p x) ]
    transpose [ ^Point x: y y: x ]
    transposed [ ^self transpose ]
    abs [ ^Point x: x abs y: y abs ]
    negated [ ^Point x: x negated y: y negated ]
    max: p [ ^Point x: (x max: p x) y: (y max: p y) ]
    min: p [ ^Point x: (x min: p x) y: (y min: p y) ]
    rounded [ ^Point x: x rounded y: y rounded ]
    truncated [ ^Point x: x truncated y: y truncated ]
    normal [ | d | d := self r. ^Point x: y negated / d y: x / d ]
    unitVector [ | d | d := self r. ^Point x: x / d y: y / d ]
    r [ ^((x * x) + (y * y)) sqrt ]
    theta [
        x = 0 ifTrue: [^y >= 0 ifTrue: [Float pi / 2] ifFalse: [Float pi / -2]].
        ^(y / x) asFloat arcTan + (x < 0 ifTrue: [Float pi] ifFalse: [0])
    ]
    angle [ ^self theta ]
    asPoint [ ^self ]
    grid: aPoint [ ^Point x: (x roundTo: aPoint x) y: (y roundTo: aPoint y) ]
    quadrant [
        x >= 0 ifTrue: [^y >= 0 ifTrue: [1] ifFalse: [4]].
        ^y >= 0 ifTrue: [2] ifFalse: [3]
    ]
]

"---- Date ----"
Date class extend [
    monthNames [ ^#(#January #February #March #April #May #June #July #August #September #October #November #December) ]
    dayNames [ ^#(#Monday #Tuesday #Wednesday #Thursday #Friday #Saturday #Sunday) ]
    nameOfDay: i [ ^self dayNames at: i ]
    nameOfMonth: i [ ^self monthNames at: i ]
    dayOfWeek: aName [ ^self dayNames indexOf: aName asSymbol ]
    indexOfMonth: aName [
        | s |
        s := aName asString.
        ^self monthNames indexOf: (self monthNames detect: [:m | m asString = s or: [(m asString copyFrom: 1 to: 3) = s]] ifNone: [^0])
    ]
    newDay: d month: m year: y [
        ^self year: y month: (m isInteger ifTrue: [m] ifFalse: [self indexOfMonth: m]) day: d
    ]
    newDay: dayOfYear year: y [ ^(self year: y month: 1 day: 1) addDays: dayOfYear - 1 ]
    fromSeconds: n [ ^self fromDays: n // 86400 ]
    daysInMonth: aName forYear: y [ ^self daysInMonth: (self indexOfMonth: aName) year: y ]
    daysInYear: y [ ^(self isLeapYear: y) ifTrue: [366] ifFalse: [365] ]
    leapYear: y [ ^self isLeapYear: y ]
    yesterday [ ^self today addDays: -1 ]
    tomorrow [ ^self today addDays: 1 ]
    epoch [ ^self year: 1970 month: 1 day: 1 ]
]
Date extend [
    daysInYear [ ^Date daysInYear: year ]
    subtractDays: n [ ^self addDays: n negated ]
    asSeconds [ ^self asDays * 86400 ]
    asUnixTime [ ^self asSeconds ]
    weekday [ ^self dayName ]
    next [ ^self addDays: 1 ]
    previous [ ^self addDays: -1 ]
    firstDayOfMonth [ ^Date year: year month: month day: 1 ]
    lastDayOfMonth [ ^Date year: year month: month day: self daysInMonth ]
    monthIndex [ ^month ]
    yyyymmdd [ ^self asNumber printString ]
    addYears: n [ ^self addMonths: n * 12 ]
    daysUntil: other [ ^other asDays - self asDays ]
    startOfWeek [ ^self addDays: 1 - self dayOfWeek ]
    endOfWeek [ ^self addDays: 7 - self dayOfWeek ]
    isToday [ ^self = Date today ]
    quarter [ ^month - 1 // 3 + 1 ]
    julianDayNumber [ ^self asDays + 2440588 ]
    asJulianDayNumber [ ^self julianDayNumber ]
    midnight [ ^DateTime date: self time: (Time hours: 0 minutes: 0 seconds: 0) ]
    asDateAndTime [ ^self midnight ]
    weekOfYear [
        | w |
        w := (self dayOfYear - self dayOfWeek + 10) // 7.
        w < 1 ifTrue: [^Date weeksInYear: year - 1].
        w > (Date weeksInYear: year) ifTrue: [^1].
        ^w
    ]
]
Date class extend [
    weeksInYear: y [
        | p |
        p := [:yy | (yy + (yy // 4) - (yy // 100) + (yy // 400)) \\ 7].
        ^(52 + (((p value: y) = 4 or: [(p value: y - 1) = 3]) ifTrue: [1] ifFalse: [0]))
    ]
]

"---- Time ----"
Time class extend [
    hour: h minute: m second: s [ ^self hours: h minutes: m seconds: s ]
    midnight [ ^self hours: 0 minutes: 0 seconds: 0 ]
    noon [ ^self hours: 12 minutes: 0 seconds: 0 ]
    zero [ ^self midnight ]
    current [ ^self now ]
    secondClock [ ^self millisecondClock // 1000 ]
    utcSecondClock [ ^self secondClock ]
    microsecondClock [ ^self millisecondClock * 1000 ]
    nanosecondClock [ ^self millisecondClock * 1000000 ]
    millisecondsSince: t0 [ ^self millisecondClock - t0 ]
    totalSeconds: n [ ^self fromSeconds: n ]
]
Time extend [
    hour24 [ ^hours ]
    hour [ ^hours ]
    hour12 [ | h | h := hours \\ 12. ^h = 0 ifTrue: [12] ifFalse: [h] ]
    minute [ ^minutes ]
    second [ ^seconds ]
    nanoSecond [ ^0 ]
    asMilliseconds [ ^self asSeconds * 1000 ]
    addTime: other [ ^Time fromSeconds: self asSeconds + other asSeconds ]
    print24 [ ^self printString ]
    hhmmss [ ^self printString ]
    isAM [ ^hours < 12 ]
    isPM [ ^hours >= 12 ]
    meridianAbbreviation [ ^self isAM ifTrue: ['AM'] ifFalse: ['PM'] ]
    asDuration [ ^Duration seconds: self asSeconds ]
    + aDuration [ ^Time fromSeconds: self asSeconds + aDuration asSeconds ]
    - other [
        ^(other isKindOf: Time)
            ifTrue: [Duration seconds: self asSeconds - other asSeconds]
            ifFalse: [Time fromSeconds: self asSeconds - other asSeconds]
    ]
    midnight [ ^Time midnight ]
    noon [ ^Time noon ]
]

"---- Duration ----"
Duration extend [
    totalSeconds [ ^secs ]
    isZero [ ^secs = 0 ]
    positive [ ^secs > 0 ]
    roundTo: aDuration [ ^Duration seconds: (secs / aDuration asSeconds) rounded * aDuration asSeconds ]
    asDays [ ^secs / 86400 ]
    asHours [ ^secs / 3600 ]
    asMinutes [ ^secs / 60 ]
    nanoSeconds [ ^((secs abs \\ 1) * 1000000000) truncated ]
    asNanoSeconds [ ^(secs * 1000000000) truncated ]
]

"---- DateTime and time zones ----"
Object subclass: TimeZone [
    | offset name abbreviation |
    TimeZone class >> offset: aDuration name: aString abbreviation: abbr [ ^self new setOffset: aDuration name: aString abbreviation: abbr ]
    TimeZone class >> utc [ ^self offset: Duration zero name: 'Universal Time' abbreviation: 'UTC' ]
    TimeZone class >> hours: h [ ^self offset: (Duration hours: h) name: 'UTC', (h >= 0 ifTrue: ['+'] ifFalse: ['']), h printString abbreviation: 'UTC', (h >= 0 ifTrue: ['+'] ifFalse: ['']), h printString ]
    setOffset: aDuration name: aString abbreviation: abbr [ offset := aDuration. name := aString. abbreviation := abbr ]
    offset [ ^offset ]
    name [ ^name ]
    abbreviation [ ^abbreviation ]
    printOn: aStream [ aStream nextPutAll: abbreviation ]
]
DateTime extend [
    | offset |
    offset [ ^offset isNil ifTrue: [Duration zero] ifFalse: [offset] ]
    offset: aDuration [ offset := aDuration ]
    utcOffset [ ^self offset ]
    timeZone [ ^TimeZone offset: self offset name: 'Local' abbreviation: self zoneString ]
    zoneString [
        | s a |
        s := self offset asSeconds.
        a := s abs // 60.
        ^(s < 0 ifTrue: ['-'] ifFalse: ['+']), (a // 60 printPaddedWith: $0 to: 2), ':', (a \\ 60 printPaddedWith: $0 to: 2)
    ]
    utcSeconds [ ^self asSeconds - self offset asSeconds ]
    asUTC [ ^(DateTime fromSeconds: self utcSeconds) offset: Duration zero; yourself ]
    asLocal [ ^self ]
    asUnixTime [ ^self utcSeconds ]
    unixTime [ ^self utcSeconds ]
    asMilliseconds [ ^self asSeconds * 1000 ]
    dayOfWeek [ ^date dayOfWeek ]
    dayName [ ^date dayName ]
    monthName [ ^date monthName ]
    hour [ ^time hours ]
    minute [ ^time minutes ]
    second [ ^time seconds ]
    midnight [ ^DateTime date: date time: Time midnight ]
    beginningOfDay [ ^self midnight ]
    startOfDay [ ^self midnight ]
    endOfDay [ ^DateTime date: date time: (Time hours: 23 minutes: 59 seconds: 59) ]
    isToday [ ^date = Date today ]
    asDateAndTime [ ^self ]
    asTimestamp [ ^self ]
    timestamp [ ^self ]
    asISOString [
        | s |
        s := WriteStream on: String new.
        s print: date; nextPut: $T; print: time.
        offset isNil ifFalse: [offset asSeconds = 0 ifTrue: [s nextPut: $Z] ifFalse: [s nextPutAll: self zoneString]].
        ^s contents
    ]
    iso8601 [ ^self asISOString ]
    printOn: aStream [ aStream nextPutAll: self asISOString ]
    < other [ ^self utcSeconds < other utcSeconds ]
    = other [ ^(other isKindOf: DateTime) and: [self utcSeconds = other utcSeconds] ]
    hash [ ^self utcSeconds ]
]
DateTime class extend [
    fromUnixTime: n [ ^self fromSeconds: n ]
    year: y [ ^self year: y month: 1 day: 1 hour: 0 minute: 0 second: 0 ]
]
