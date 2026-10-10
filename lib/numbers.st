"numbers.st - Number, Integer, Float, Fraction"

Number extend [
    isNumber [ ^true ]
    generality [ ^self subclassResponsibility ]
    coerce: n [ ^self subclassResponsibility ]
    retry: sel coercing: n [
        | g1 g2 |
        g1 := self generality.
        g2 := n generality.
        g1 = g2 ifTrue: [^self error: 'retry:coercing: failed'].
        g1 > g2 ifTrue: [^self perform: sel with: (self coerce: n)].
        ^(n coerce: self) perform: sel with: n
    ]
    zeroDivide [ ^ZeroDivide new dividend: self; receiver: self; signal: 'The program attempted to divide a number by zero' ]
    + n [ <primitive: add> ^self retry: #+ coercing: n ]
    - n [ <primitive: sub> ^self retry: #- coercing: n ]
    * n [ <primitive: mul> ^self retry: #* coercing: n ]
    / n [
        <primitive: div>
        n = 0 ifTrue: [^self zeroDivide].
        (self isInteger and: [n isInteger]) ifTrue: [^(Fraction numerator: self denominator: n) normalized].
        ^self retry: #/ coercing: n
    ]
    // n [ <primitive: idiv> n = 0 ifTrue: [^self zeroDivide]. ^self retry: #// coercing: n ]
    \\ n [ <primitive: mod> n = 0 ifTrue: [^self zeroDivide]. ^self retry: #\\ coercing: n ]
    rem: n [ <primitive: rem> n = 0 ifTrue: [^self zeroDivide]. ^self - ((self quo: n) * n) ]
    quo: n [ <primitive: quo> n = 0 ifTrue: [^self zeroDivide]. ^(self / n) truncated ]
    < n [ <primitive: lt> ^self retry: #< coercing: n ]
    > n [ <primitive: gt> ^self retry: #> coercing: n ]
    <= n [ <primitive: le> ^self retry: #<= coercing: n ]
    >= n [ <primitive: ge> ^self retry: #>= coercing: n ]
    = n [ <primitive: eq> ^false ]
    ~= n [ <primitive: ne> ^true ]
    hash [ ^self truncated ]
    @ y [ ^Point x: self y: y ]
    abs [ ^self < 0 ifTrue: [self negated] ifFalse: [self] ]
    negated [ ^0 - self ]
    sign [ ^self < 0 ifTrue: [-1] ifFalse: [self > 0 ifTrue: [1] ifFalse: [0]] ]
    negative [ ^self < 0 ]
    positive [ ^self >= 0 ]
    strictlyPositive [ ^self > 0 ]
    isZero [ ^self = 0 ]
    squared [ ^self * self ]
    reciprocal [ ^1 / self ]
    even [ ^self truncated even ]
    odd [ ^self truncated odd ]
    max: n [ ^self > n ifTrue: [self] ifFalse: [n] ]
    min: n [ ^self < n ifTrue: [self] ifFalse: [n] ]
    between: a and: b [ ^self >= a and: [self <= b] ]
    isRational [ ^false ]
    asNumber [ ^self ]
    asInteger [ ^self truncated ]
    asFloat [ <primitive: asFloat> ^self asFloatD ]
    asFloatD [ <primitive: asFloat> ]
    truncated [ <primitive: truncated> ]
    floor [ <primitive: floor> ]
    ceiling [ <primitive: ceiling> ]
    rounded [ <primitive: rounded> ]
    roundTo: n [ ^(self / n) rounded * n ]
    truncateTo: n [ ^(self quo: n) * n ]
    integerPart [ ^self truncated ]
    fractionPart [ ^self - self truncated ]
    primFloat: k with: x [ <primitive: floatOp> ^self error: 'invalid argument for math function' ]
    sqrt [ ^self primFloat: 1 with: 0 ]
    sin [ ^self primFloat: 2 with: 0 ]
    cos [ ^self primFloat: 3 with: 0 ]
    tan [ ^self primFloat: 4 with: 0 ]
    arcTan [ ^self primFloat: 5 with: 0 ]
    arcSin [ ^self primFloat: 9 with: 0 ]
    arcCos [ ^self primFloat: 10 with: 0 ]
    ln [ ^self primFloat: 6 with: 0 ]
    exp [ ^self primFloat: 7 with: 0 ]
    log [ ^self primFloat: 8 with: 0 ]
    log: base [ ^self ln / base ln ]
    raisedTo: n [
        | r b e |
        n isInteger ifFalse: [^self primFloat: 11 with: n asFloat].
        n < 0 ifTrue: [^(self raisedTo: n negated) reciprocal].
        r := 1. b := self. e := n.
        [e > 0] whileTrue: [
            e odd ifTrue: [r := r * b].
            e := e // 2.
            e > 0 ifTrue: [b := b * b]].
        ^r
    ]
    ** n [ ^self raisedTo: n ]
    to: stop [ ^Interval from: self to: stop by: 1 ]
    to: stop by: step [ ^Interval from: self to: stop by: step ]
    to: stop do: aBlock [
        | i |
        i := self.
        [i <= stop] whileTrue: [aBlock value: i. i := i + 1]
    ]
    to: stop by: step do: aBlock [
        | i |
        i := self.
        step > 0
            ifTrue: [[i <= stop] whileTrue: [aBlock value: i. i := i + step]]
            ifFalse: [[i >= stop] whileTrue: [aBlock value: i. i := i + step]]
    ]
    printOn: aStream [ aStream nextPutAll: self printString ]
    displayString [ ^self printString ]
    displayNl [ Transcript showCr: self printString ]
    printNl [ Transcript showCr: self printString ]
    asString [ ^self printString ]
    asCharacter [ ^Character value: self ]
    isNaN [ ^false ]
    copy [ ^self ]
    shallowCopy [ ^self ]
    deepCopy [ ^self ]
]

Integer extend [
    generality [ ^100 ]
    coerce: n [ ^n truncated ]
    isInteger [ ^true ]
    isRational [ ^true ]
    printString [ <primitive: printString> ]
    printString: base [ <primitive: printRadix> ^self printStringRadix: base ]
    printStringRadix: base [
        | n s digs |
        base = 10 ifTrue: [^self printString].
        n := self abs. s := ''.
        digs := '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'.
        n = 0 ifTrue: [^'0'].
        [n > 0] whileTrue: [s := (String with: (digs at: n \\ base + 1)), s. n := n // base].
        ^self < 0 ifTrue: ['-', s] ifFalse: [s]
    ]
    radix: base [ ^self printString: base ]
    printRadix: base [ ^base printString, 'r', (self printString: base) ]
    storeString: base [ ^self printRadix: base ]
    printPaddedWith: ch to: width [
        | s |
        s := self printString.
        ^s size >= width ifTrue: [s] ifFalse: [(String new: width - s size withAll: ch), s]
    ]
    printString: base paddedWith: ch to: width [
        | s |
        s := self printString: base.
        ^s size >= width ifTrue: [s] ifFalse: [(String new: width - s size withAll: ch), s]
    ]
    numerator [ ^self ]
    denominator [ ^1 ]
    truncated [ ^self ]
    floor [ ^self ]
    ceiling [ ^self ]
    rounded [ ^self ]
    asInteger [ ^self ]
    even [ ^self \\ 2 = 0 ]
    odd [ ^self \\ 2 = 1 ]
    isProbablyPrime [ ^self isPrime ]
    isPrime [
        | i |
        self < 2 ifTrue: [^false].
        self < 4 ifTrue: [^true].
        self even ifTrue: [^false].
        i := 3.
        [i * i <= self] whileTrue: [self \\ i = 0 ifTrue: [^false]. i := i + 2].
        ^true
    ]
    factorial [
        | r |
        self < 0 ifTrue: [^self error: 'factorial: receiver is negative'].
        r := 1.
        2 to: self do: [:i | r := r * i].
        ^r
    ]
    gcd: n [
        | a b t |
        a := self abs. b := n abs.
        [b = 0] whileFalse: [t := a \\ b. a := b. b := t].
        ^a
    ]
    lcm: n [ ^(self = 0 or: [n = 0]) ifTrue: [0] ifFalse: [(self * n) abs // (self gcd: n)] ]
    sqrtFloor [ <primitive: sqrtFloor> ^(self asFloat sqrt) truncated ]
    timesRepeat: aBlock [
        | i |
        i := 1.
        [i <= self] whileTrue: [aBlock value. i := i + 1]
    ]
    bitAnd: n [ <primitive: bitAnd> ^self error: 'bitAnd: requires non-negative integers' ]
    bitOr: n [ <primitive: bitOr> ^self error: 'bitOr: requires non-negative integers' ]
    bitXor: n [ <primitive: bitXor> ^self error: 'bitXor: requires non-negative integers' ]
    bitShift: n [ <primitive: bitShift> ]
    << n [ ^self bitShift: n ]
    >> n [ ^self bitShift: n negated ]
    bitAt: i [ ^(self bitShift: 1 - i) bitAnd: 1 ]
    isBitSet: i [ ^(self bitAt: i) = 1 ]
    asFloat [ <primitive: asFloat> ]
    numberOfDigits [ ^self abs printString size ]
    digitAt: i [ ^(self abs printString at: self abs printString size - i + 1) digitValue ]
    reverse [ ^self printString reverse asNumber ]
    isPalindrome [ ^self printString = self printString reverse ]
    asWords [ ^self printString ]
    ceilingLog: base [ | n r | n := 1. r := 0. [n < self] whileTrue: [n := n * base. r := r + 1]. ^r ]
    sqrt [ ^self primFloat: 1 with: 0 ]
    hash [ ^self ]
]

SmallInteger class extend [
    maxVal [ ^999999999999999999 ]
    minVal [ ^-999999999999999999 ]
]

Number class extend [
    readFrom: aString [ ^aString asNumber ]
]

Integer class extend [
    readFrom: aString [ ^aString asInteger ]
]

LargeInteger extend [
    generality [ ^200 ]
    coerce: n [ ^n truncated ]
    hash [ ^self printString hash ]
]

Float extend [
    generality [ ^400 ]
    coerce: n [ ^n asFloat ]
    printString [ <primitive: printString> ]
    asFloat [ ^self ]
    truncated [ <primitive: truncated> ^self error: 'cannot truncate this number' ]
    floor [ <primitive: floor> ^self error: 'cannot truncate this number' ]
    ceiling [ <primitive: ceiling> ^self error: 'cannot truncate this number' ]
    rounded [ <primitive: rounded> ^self error: 'cannot truncate this number' ]
    isFloat [ ^true ]
    isNaN [ ^self ~= self ]
    isInfinite [ ^false ]
    even [ ^self truncated even ]
    odd [ ^self truncated odd ]
    hash [ ^self fractionPart = 0.0 ifTrue: [self truncated hash] ifFalse: [self printString hash] ]
    roundTo: n [ ^(self / n) rounded * n ]
    asExactFraction [ ^self asFraction ]
    asFraction [
        | d n |
        d := 1. n := self.
        [n fractionPart = 0.0 or: [d > 1000000000]] whileFalse: [n := n * 10. d := d * 10].
        ^(Fraction numerator: n truncated denominator: d) normalized
    ]
    printOn: aStream [ aStream nextPutAll: self printString ]
]

FloatD class extend [
    pi [ ^3.141592653589793 ]
    e [ ^2.718281828459045 ]
    infinity [ ^1.0e308 * 10.0 ]
    nan [ ^0.0 / 0.0 ]
    epsilon [ ^2.220446049250313e-16 ]
]
Float class extend [
    pi [ ^3.141592653589793 ]
    e [ ^2.718281828459045 ]
    epsilon [ ^2.220446049250313e-16 ]
]

Fraction class extend [
    numerator: n denominator: d [ ^self new setNumerator: n denominator: d ]
]

Fraction extend [
    | numerator denominator |
    setNumerator: n denominator: d [ numerator := n. denominator := d ]
    numerator [ ^numerator ]
    denominator [ ^denominator ]
    isFraction [ ^true ]
    isRational [ ^true ]
    generality [ ^300 ]
    coerce: n [ ^Fraction numerator: n truncated denominator: 1 ]
    normalized [
        | g n d |
        denominator = 0 ifTrue: [^self zeroDivide].
        n := numerator. d := denominator.
        d < 0 ifTrue: [n := n negated. d := d negated].
        g := n gcd: d.
        g > 1 ifTrue: [n := n // g. d := d // g].
        d = 1 ifTrue: [^n].
        ^Fraction numerator: n denominator: d
    ]
    + n [
        n isFraction ifTrue: [^(Fraction numerator: numerator * n denominator + (n numerator * denominator) denominator: denominator * n denominator) normalized].
        n isInteger ifTrue: [^(Fraction numerator: numerator + (n * denominator) denominator: denominator) normalized].
        ^self retry: #+ coercing: n
    ]
    - n [
        n isFraction ifTrue: [^(Fraction numerator: numerator * n denominator - (n numerator * denominator) denominator: denominator * n denominator) normalized].
        n isInteger ifTrue: [^(Fraction numerator: numerator - (n * denominator) denominator: denominator) normalized].
        ^self retry: #- coercing: n
    ]
    * n [
        n isFraction ifTrue: [^(Fraction numerator: numerator * n numerator denominator: denominator * n denominator) normalized].
        n isInteger ifTrue: [^(Fraction numerator: numerator * n denominator: denominator) normalized].
        ^self retry: #* coercing: n
    ]
    / n [
        n = 0 ifTrue: [^self zeroDivide].
        n isFraction ifTrue: [^(Fraction numerator: numerator * n denominator denominator: denominator * n numerator) normalized].
        n isInteger ifTrue: [^(Fraction numerator: numerator denominator: denominator * n) normalized].
        ^self retry: #/ coercing: n
    ]
    // n [ ^(self / n) floor ]
    \\ n [ ^self - (self // n * n) ]
    < n [
        n isFraction ifTrue: [^numerator * n denominator < (n numerator * denominator)].
        n isInteger ifTrue: [^numerator < (n * denominator)].
        ^self asFloat < n
    ]
    > n [ ^n < self ]
    <= n [ ^(n < self) not ]
    >= n [ ^(self < n) not ]
    = n [
        n isFraction ifTrue: [^numerator = n numerator and: [denominator = n denominator]].
        n isFloat ifTrue: [^self asFloat = n].
        ^false
    ]
    ~= n [ ^(self = n) not ]
    hash [ ^numerator hash bitXor: denominator hash ]
    negated [ ^Fraction numerator: numerator negated denominator: denominator ]
    reciprocal [ ^(Fraction numerator: denominator denominator: numerator) normalized ]
    abs [ ^Fraction numerator: numerator abs denominator: denominator ]
    truncated [ ^numerator quo: denominator ]
    floor [ ^numerator // denominator ]
    ceiling [ ^(numerator // denominator) + 1 ]
    rounded [ ^(self + (1/2)) floor ]
    asFloat [ ^numerator asFloat / denominator asFloat ]
    sqrt [ ^self asFloat sqrt ]
    printString [ ^numerator printString, '/', denominator printString ]
    printOn: aStream [ aStream nextPutAll: self printString ]
    storeString [ ^'(Fraction numerator: ', numerator printString, ' denominator: ', denominator printString, ')' ]
]
