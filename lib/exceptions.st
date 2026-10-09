"exceptions.st - Exception, Error, Warning, ZeroDivide, ..."

Signal extend [
    | messageText handlerIdx sigId receiver |
]

Exception class extend [
    new [ ^self basicNew ]
    signal [ ^self new signal ]
    signal: msg [ ^self new signal: msg ]
    signalOn: value [ ^self new receiver: value; signal ]
    signalOn: value reason: msg [ ^self new receiver: value; signal: msg ]
    , other [ ^Array with: self with: other ]
]

Exception extend [
    receiver [ ^receiver ]
    receiver: r [ receiver := r ]
    signalOn: r [ receiver := r. ^self signal ]
    value [ ^receiver ]
    messageText [ ^messageText ifNil: [self description] ]
    messageText: t [ messageText := t ]
    description [ ^'An exception has occurred' ]
    isResumable [ ^true ]
    isNested [ ^false ]
    signal [ <primitive: signal> ]
    signal: msg [ messageText := msg. ^self signal ]
    primResume: v [ <primitive: resume> ]
    primReturn: v [ <primitive: return> ]
    primRetry [ <primitive: retry> ]
    primPass [ <primitive: pass> ]
    primOuter [ <primitive: outer> ]
    primInfo: n [ <primitive: excInfo> ]
    resume [ ^self resume: nil ]
    resume: v [
        self isResumable ifFalse: [^SystemExceptions.WrongClass signalOn: self reason: 'Exception>>#resume: non-resumable instance of ', self class name].
        ^self primResume: v
    ]
    return [ ^self primReturn: nil ]
    return: v [ ^self primReturn: v ]
    retry [ ^self primRetry ]
    pass [ ^self primPass ]
    outer [ ^self primOuter ]
    retryUsing: aBlock [ ^self primReturn: aBlock value ]
    signalingContext [ ^nil ]
    selector [ ^nil ]
    argument [ ^nil ]
    context [ ^nil ]
    creator [ ^receiver ]
    defaultAction [
        Transcript showCr: 'Object: ', receiver printString, ' error: ', self messageText.
        ^self primInfo: 1
    ]
    printOn: aStream [ aStream nextPutAll: self class article, ' ', self class name ]
]

Error extend [
    isResumable [ ^false ]
    defaultAction [
        Transcript showCr: 'Object: ', receiver printString, ' error: ', self messageText.
        ^self primInfo: 1
    ]
]

Warning extend [
    defaultAction [
        Transcript showCr: 'Object: ', receiver printString, ' error: ', self messageText.
        self primInfo: 2.
        ^nil
    ]
]

ZeroDivide extend [
    | dividend |
    dividend [ ^dividend ]
    dividend: d [ dividend := d ]
    description [ ^'The program attempted to divide a number by zero' ]
]

MessageNotUnderstood extend [
    | message |
    message [ ^message ]
    message: m [ message := m. messageText := 'did not understand #', m selector ]
    isResumable [ ^true ]
    return: v [ ^self primReturn: v ]
]

InvalidValue class extend [
    signalOn: value reason: msg [ ^self new receiver: value; signal: msg ]
]
InvalidArgument class extend [
    signalOn: value reason: msg [ ^self new receiver: value; signal: msg ]
]
IndexOutOfRange class extend [
    signalOn: coll withIndex: i [
        ^self new receiver: coll; signal: 'Index value ', i printString, ' out of range: only ', coll size printString, ' elements'
    ]
]
NotFound class extend [
    signalOn: value what: what reason: msg [ ^self new receiver: value; signal: msg ]
]
EmptyCollection class extend [
    signalOn: coll [ ^self new receiver: coll; signal: 'access beyond end of object' ]
]
WrongClass class extend [
    signalOn: value reason: msg [ ^self new receiver: value; signal: msg ]
]

Error subclass: EndOfStream [
    description [ ^'end of stream reached' ]
]
EndOfStream class extend [
    signalOn: stream [ ^self new receiver: stream; signal: 'end of stream reached' ]
]
InvalidValue subclass: ArgumentOutOfRange [ ]
ArgumentOutOfRange class extend [
    signalOn: value mustBeBetween: lo and: hi [
        ^self new receiver: value; signal: 'Invalid argument ', value printString, ': argument must be between ', lo printString, ' and ', hi printString
    ]
]
NotFound subclass: ElementNotFound [ ]
Error subclass: ProcessTerminated [ ]

BlockClosure extend [
    numArgs [ <primitive: blockNumArgs> ]
    value [ <primitive: blockValue> ]
    value: a [ <primitive: blockValue> ]
    value: a value: b [ <primitive: blockValue> ]
    value: a value: b value: c [ <primitive: blockValue> ]
    value: a value: b value: c value: d [ <primitive: blockValue> ]
    valueWithArguments: args [ <primitive: blockValueArgs> ]
    whileTrue: aBlock [ <primitive: whileTrue> ]
    whileFalse: aBlock [ <primitive: whileFalse> ]
    whileTrue [ ^self whileTrue: [] ]
    whileFalse [ ^self whileFalse: [] ]
    repeat [ [self value. true] whileTrue ]
    on: exc do: handler [ <primitive: onDo> ]
    on: e1 do: h1 on: e2 do: h2 [ ^[self on: e1 do: h1] on: e2 do: h2 ]
    ensure: aBlock [ <primitive: ensure> ]
    ifCurtailed: aBlock [ <primitive: ifCurtailed> ]
    valueWithUnwind [ ^self value ]
    fork [ ^self value ]
    printOn: aStream [ aStream nextPutAll: 'a BlockClosure' ]
]
