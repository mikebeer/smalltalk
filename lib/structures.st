"structures.st - number formatting, Complex, Stack, Queue, Matrix"

Number extend [
    printShowingDecimalPlaces: n [
        | k s neg |
        k := (self abs * (10 raisedTo: n)) rounded.
        neg := self < 0 and: [k > 0].
        s := k printString.
        s size <= n ifTrue: [s := (String new: n - s size + 1 withAll: $0), s].
        n = 0 ifTrue: [^(neg ifTrue: ['-'] ifFalse: ['']), s].
        ^(neg ifTrue: ['-'] ifFalse: ['']), (s copyFrom: 1 to: s size - n), '.', (s copyFrom: s size - n + 1 to: s size)
    ]
    i [ ^Complex real: 0 imag: self ]
]

Integer extend [
    asStringWithCommas [
        | s ws n |
        s := self abs printString.
        ws := WriteStream on: String new.
        self < 0 ifTrue: [ws nextPut: $-].
        n := s size.
        1 to: n do: [:k |
            ws nextPut: (s at: k).
            (n - k > 0 and: [(n - k) \\ 3 = 0]) ifTrue: [ws nextPut: $,]].
        ^ws contents
    ]
]

Number subclass: Complex [
    | re im |
    Complex class >> real: r imag: i [ ^self new setReal: r imag: i ]
    Complex class >> new [ ^self basicNew ]
    setReal: r imag: i [ re := r. im := i ]
    real [ ^re ]
    imaginary [ ^im ]
    generality [ ^200 ]
    coerce: n [ ^Complex real: n imag: 0 ]
    + n [ | c | c := n isKindOf: Complex. ^c ifTrue: [Complex real: re + n real imag: im + n imaginary] ifFalse: [Complex real: re + n imag: im] ]
    - n [ | c | c := n isKindOf: Complex. ^c ifTrue: [Complex real: re - n real imag: im - n imaginary] ifFalse: [Complex real: re - n imag: im] ]
    * n [
        (n isKindOf: Complex) ifFalse: [^Complex real: re * n imag: im * n].
        ^Complex real: (re * n real) - (im * n imaginary) imag: (re * n imaginary) + (im * n real)
    ]
    / n [
        | d |
        (n isKindOf: Complex) ifFalse: [^Complex real: re / n imag: im / n].
        d := (n real * n real) + (n imaginary * n imaginary).
        ^Complex real: ((re * n real) + (im * n imaginary)) / d imag: ((im * n real) - (re * n imaginary)) / d
    ]
    = n [ ^(n isKindOf: Complex) and: [re = n real and: [im = n imaginary]] ]
    hash [ ^re hash bitXor: im hash ]
    negated [ ^Complex real: re negated imag: im negated ]
    conjugate [ ^Complex real: re imag: im negated ]
    abs [ ^((re * re) + (im * im)) sqrt ]
    squaredNorm [ ^(re * re) + (im * im) ]
    isZero [ ^re = 0 and: [im = 0] ]
    isComplex [ ^true ]
    printOn: aStream [
        aStream print: re.
        im < 0 ifTrue: [aStream nextPut: $-; print: im negated] ifFalse: [aStream nextPut: $+; print: im].
        aStream nextPut: $i
    ]
]
Object extend [ isComplex [ ^false ] ]

Object subclass: Stack [
    | items |
    Stack class >> new [ ^self basicNew init ]
    init [ items := OrderedCollection new ]
    push: x [ items addLast: x. ^x ]
    pop [ items isEmpty ifTrue: [^self error: 'stack is empty']. ^items removeLast ]
    top [ items isEmpty ifTrue: [^self error: 'stack is empty']. ^items last ]
    isEmpty [ ^items isEmpty ]
    notEmpty [ ^items notEmpty ]
    size [ ^items size ]
    do: aBlock [ items reverseDo: aBlock ]
    removeAll [ items removeAll ]
    printOn: aStream [ aStream nextPutAll: 'Stack'; print: items asArray ]
]

Object subclass: Queue [
    | items |
    Queue class >> new [ ^self basicNew init ]
    init [ items := OrderedCollection new ]
    enqueue: x [ items addLast: x. ^x ]
    dequeue [ items isEmpty ifTrue: [^self error: 'queue is empty']. ^items removeFirst ]
    front [ items isEmpty ifTrue: [^self error: 'queue is empty']. ^items first ]
    isEmpty [ ^items isEmpty ]
    notEmpty [ ^items notEmpty ]
    size [ ^items size ]
    do: aBlock [ items do: aBlock ]
    removeAll [ items removeAll ]
    printOn: aStream [ aStream nextPutAll: 'Queue'; print: items asArray ]
]

Object subclass: Matrix [
    | rows cols data |
    Matrix class >> rows: r columns: c [ ^self basicNew setRows: r columns: c ]
    Matrix class >> rows: r columns: c withAll: v [ ^(self rows: r columns: c) fill: v ]
    Matrix class >> identity: n [
        | m |
        m := self rows: n columns: n withAll: 0.
        1 to: n do: [:i | m at: i at: i put: 1].
        ^m
    ]
    Matrix class >> fromRows: anArray [
        | m |
        m := self rows: anArray size columns: anArray first size.
        1 to: anArray size do: [:i | 1 to: anArray first size do: [:j | m at: i at: j put: ((anArray at: i) at: j)]].
        ^m
    ]
    setRows: r columns: c [ rows := r. cols := c. data := Array new: r * c withAll: 0 ]
    fill: v [ 1 to: data size do: [:i | data at: i put: v] ]
    rows [ ^rows ]
    columns [ ^cols ]
    at: i at: j [ ^data at: (i - 1 * cols) + j ]
    at: i at: j put: v [ ^data at: (i - 1 * cols) + j put: v ]
    row: i [ ^(1 to: cols) collect: [:j | self at: i at: j] ]
    column: j [ ^(1 to: rows) collect: [:i | self at: i at: j] ]
    asArray [ ^(1 to: rows) collect: [:i | self row: i] ]
    + m [
        | r |
        r := Matrix rows: rows columns: cols.
        1 to: rows do: [:i | 1 to: cols do: [:j | r at: i at: j put: (self at: i at: j) + (m at: i at: j)]].
        ^r
    ]
    - m [
        | r |
        r := Matrix rows: rows columns: cols.
        1 to: rows do: [:i | 1 to: cols do: [:j | r at: i at: j put: (self at: i at: j) - (m at: i at: j)]].
        ^r
    ]
    * m [
        | r s |
        (m isKindOf: Matrix) ifFalse: [
            r := Matrix rows: rows columns: cols.
            1 to: rows do: [:i | 1 to: cols do: [:j | r at: i at: j put: (self at: i at: j) * m]].
            ^r].
        cols = m rows ifFalse: [^self error: 'dimension mismatch'].
        r := Matrix rows: rows columns: m columns.
        1 to: rows do: [:i | 1 to: m columns do: [:j |
            s := 0.
            1 to: cols do: [:k | s := s + ((self at: i at: k) * (m at: k at: j))].
            r at: i at: j put: s]].
        ^r
    ]
    transposed [
        | r |
        r := Matrix rows: cols columns: rows.
        1 to: rows do: [:i | 1 to: cols do: [:j | r at: j at: i put: (self at: i at: j)]].
        ^r
    ]
    trace [ | s | s := 0. 1 to: (rows min: cols) do: [:i | s := s + (self at: i at: i)]. ^s ]
    minor: i column: j [
        | r ri rj |
        r := Matrix rows: rows - 1 columns: cols - 1.
        ri := 0.
        1 to: rows do: [:a |
            a = i ifFalse: [
                ri := ri + 1. rj := 0.
                1 to: cols do: [:b | b = j ifFalse: [rj := rj + 1. r at: ri at: rj put: (self at: a at: b)]]]].
        ^r
    ]
    determinant [
        | d sign |
        rows = cols ifFalse: [^self error: 'matrix is not square'].
        rows = 1 ifTrue: [^self at: 1 at: 1].
        rows = 2 ifTrue: [^((self at: 1 at: 1) * (self at: 2 at: 2)) - ((self at: 1 at: 2) * (self at: 2 at: 1))].
        d := 0. sign := 1.
        1 to: cols do: [:j |
            d := d + (sign * (self at: 1 at: j) * (self minor: 1 column: j) determinant).
            sign := sign negated].
        ^d
    ]
    = m [ ^(m isKindOf: Matrix) and: [rows = m rows and: [cols = m columns and: [self asArray = m asArray]]] ]
    hash [ ^data hash ]
    printOn: aStream [
        1 to: rows do: [:i |
            aStream nextPut: $[.
            1 to: cols do: [:j | j > 1 ifTrue: [aStream space]. aStream print: (self at: i at: j)].
            aStream nextPut: $].
            i < rows ifTrue: [aStream nl]]
    ]
]

Object subclass: LinkNode [
    | value next prev |
    value [ ^value ]
    value: v [ value := v ]
    next [ ^next ]
    next: n [ next := n ]
    prev [ ^prev ]
    prev: p [ prev := p ]
]

Collection subclass: LinkedList [
    | head tail count |
    LinkedList class >> new [ ^self basicNew init ]
    init [ count := 0 ]
    size [ ^count ]
    isEmpty [ ^count = 0 ]
    add: x [ ^self addLast: x ]
    addLast: x [
        | n |
        n := LinkNode new value: x; yourself.
        tail isNil ifTrue: [head := n] ifFalse: [tail next: n. n prev: tail].
        tail := n.
        count := count + 1.
        ^x
    ]
    addFirst: x [
        | n |
        n := LinkNode new value: x; yourself.
        head isNil ifTrue: [tail := n] ifFalse: [head prev: n. n next: head].
        head := n.
        count := count + 1.
        ^x
    ]
    first [ count = 0 ifTrue: [^self emptyCheck]. ^head value ]
    last [ count = 0 ifTrue: [^self emptyCheck]. ^tail value ]
    removeFirst [
        | v |
        count = 0 ifTrue: [^self emptyCheck].
        v := head value.
        head := head next.
        head isNil ifTrue: [tail := nil] ifFalse: [head prev: nil].
        count := count - 1.
        ^v
    ]
    removeLast [
        | v |
        count = 0 ifTrue: [^self emptyCheck].
        v := tail value.
        tail := tail prev.
        tail isNil ifTrue: [head := nil] ifFalse: [tail next: nil].
        count := count - 1.
        ^v
    ]
    remove: x ifAbsent: aBlock [
        | n |
        n := head.
        [n notNil] whileTrue: [
            n value = x ifTrue: [
                n prev isNil ifTrue: [head := n next] ifFalse: [n prev next: n next].
                n next isNil ifTrue: [tail := n prev] ifFalse: [n next prev: n prev].
                count := count - 1.
                ^x].
            n := n next].
        ^aBlock value
    ]
    do: aBlock [
        | n |
        n := head.
        [n notNil] whileTrue: [aBlock value: n value. n := n next]
    ]
    reverseDo: aBlock [
        | n |
        n := tail.
        [n notNil] whileTrue: [aBlock value: n value. n := n prev]
    ]
    reverse [ | r | r := LinkedList new. self do: [:e | r addFirst: e]. ^r ]
    copyEmpty [ ^LinkedList new ]
    asArray [ | r i | r := Array new: count. i := 0. self do: [:e | r at: (i := i + 1) put: e]. ^r ]
    printOn: aStream [ aStream nextPutAll: 'LinkedList'; print: self asArray ]
]

Object subclass: Stopwatch [
    | started elapsed |
    Stopwatch class >> new [ ^self basicNew reset ]
    Stopwatch class >> time: aBlock [ ^self new start; run: aBlock; stop; elapsed ]
    reset [ elapsed := 0. started := nil ]
    start [ started := Time millisecondClock ]
    stop [ started notNil ifTrue: [elapsed := elapsed + (Time millisecondClock - started). started := nil] ]
    run: aBlock [ aBlock value ]
    elapsed [ ^started isNil ifTrue: [elapsed] ifFalse: [elapsed + (Time millisecondClock - started)] ]
    isRunning [ ^started notNil ]
    printOn: aStream [ aStream nextPutAll: 'Stopwatch('; print: self elapsed; nextPutAll: ' ms)' ]
]

String extend [
    match: text [ ^self matchFrom: 1 text: text at: 1 ]
    matchFrom: pi text: t at: ti [
        | p |
        p := pi.
        [p <= self size] whileTrue: [
            (self at: p) = $*
                ifTrue: [
                    p = self size ifTrue: [^true].
                    ti to: t size + 1 do: [:k | (self matchFrom: p + 1 text: t at: k) ifTrue: [^true]].
                    ^false]
                ifFalse: [
                    ti > t size ifTrue: [^false].
                    ((self at: p) = $# or: [(self at: p) asLowercase = (t at: ti) asLowercase]) ifFalse: [^false]].
            ^self matchFrom: p + 1 text: t at: ti + 1].
        ^ti > t size
    ]
    fromJson [ ^(JsonParser on: self) parseValue ]
]

Object subclass: JsonParser [
    | s |
    JsonParser class >> on: str [ ^self basicNew setStream: (ReadStream on: str) ]
    setStream: x [ s := x ]
    skipWs [ [s atEnd not and: [s peek isSeparator]] whileTrue: [s next] ]
    parseValue [
        | c |
        self skipWs.
        s atEnd ifTrue: [^self error: 'unexpected end of JSON'].
        c := s peek.
        c = ${ ifTrue: [^self parseObject].
        c = $[ ifTrue: [^self parseArray].
        c = $" ifTrue: [^self parseString].
        c = $t ifTrue: [self expect: 'true'. ^true].
        c = $f ifTrue: [self expect: 'false'. ^false].
        c = $n ifTrue: [self expect: 'null'. ^nil].
        ^self parseNumber
    ]
    expect: w [ w do: [:ch | (s atEnd or: [s next ~= ch]) ifTrue: [^self error: 'bad JSON literal']] ]
    parseNumber [
        | ws |
        ws := WriteStream on: String new.
        [s atEnd not and: ['-+.eE0123456789' includes: s peek]] whileTrue: [ws nextPut: s next].
        ws contents isEmpty ifTrue: [^self error: 'bad JSON value'].
        ^ws contents asNumber
    ]
    parseString [
        | ws c e |
        s next.
        ws := WriteStream on: String new.
        [s atEnd ifTrue: [^self error: 'unterminated JSON string'].
         c := s next.
         c = $"] whileFalse: [
            c = $\
                ifTrue: [
                    e := s next.
                    e = $n ifTrue: [ws nextPut: (Character value: 10)] ifFalse: [
                    e = $t ifTrue: [ws nextPut: (Character value: 9)] ifFalse: [
                    e = $r ifTrue: [ws nextPut: (Character value: 13)] ifFalse: [
                    ws nextPut: e]]]]
                ifFalse: [ws nextPut: c]].
        ^ws contents
    ]
    parseArray [
        | r |
        s next.
        r := OrderedCollection new.
        self skipWs.
        s peek = $] ifTrue: [s next. ^r asArray].
        [r add: self parseValue. self skipWs. s next = $,] whileTrue.
        ^r asArray
    ]
    parseObject [
        | d k |
        s next.
        d := Dictionary new.
        self skipWs.
        s peek = $} ifTrue: [s next. ^d].
        [self skipWs. k := self parseString. self skipWs. s next.
         d at: k put: self parseValue.
         self skipWs.
         s next = $,] whileTrue.
        ^d
    ]
]

Object extend [
    asJson [ | ws | ws := WriteStream on: String new. self jsonOn: ws. ^ws contents ]
    jsonOn: aStream [ aStream print: self displayString ]
]
UndefinedObject extend [ jsonOn: aStream [ aStream nextPutAll: 'null' ] ]
Boolean extend [ jsonOn: aStream [ aStream nextPutAll: self printString ] ]
Number extend [ jsonOn: aStream [ aStream nextPutAll: self printString ] ]
Fraction extend [ jsonOn: aStream [ aStream nextPutAll: self asFloat printString ] ]
String extend [
    jsonOn: aStream [
        aStream nextPut: $".
        self do: [:c |
            c = $" ifTrue: [aStream nextPutAll: '\"'] ifFalse: [
            c = $\ ifTrue: [aStream nextPutAll: '\\'] ifFalse: [
            c value = 10 ifTrue: [aStream nextPutAll: '\n'] ifFalse: [
            c value = 13 ifTrue: [aStream nextPutAll: '\r'] ifFalse: [
            c value = 9 ifTrue: [aStream nextPutAll: '\t'] ifFalse: [
            aStream nextPut: c]]]]]].
        aStream nextPut: $"
    ]
]
Symbol extend [ jsonOn: aStream [ self asString jsonOn: aStream ] ]
Collection extend [
    jsonOn: aStream [
        | first |
        first := true.
        aStream nextPut: $[.
        self do: [:e | first ifFalse: [aStream nextPut: $,]. first := false. e jsonOn: aStream].
        aStream nextPut: $]
    ]
]
Dictionary extend [
    jsonOn: aStream [
        | first |
        first := true.
        aStream nextPut: ${.
        (self keys asSortedCollection: [:a :b | a displayString <= b displayString]) do: [:k |
            first ifFalse: [aStream nextPut: $,]. first := false.
            k displayString jsonOn: aStream.
            aStream nextPut: $:.
            (self at: k) jsonOn: aStream].
        aStream nextPut: $}
    ]
]
