"process.st - cooperative Processes, Semaphore, SharedQueue, Delay.
 There is one real thread.  A forked Process starts when the running one waits, yields,
 or when the current top-level statement ends; a Process that waits runs the other ready
 Processes (nested) until its condition holds."

Exception subclass: ProcessTerminated [
]

Object subclass: Process [
    | block priority name state terminateRequested |
    Process class >> on: aBlock priority: p [ ^self basicNew setBlock: aBlock priority: p ]
    setBlock: aBlock priority: p [ block := aBlock. priority := p. state := #suspended. terminateRequested := false. name := nil ]
    block [ ^block ]
    priority [ ^priority ]
    priority: p [ priority := p ]
    name [ ^name ]
    name: aString [ name := aString ]
    state [ ^state ]
    setState: aSymbol [ state := aSymbol ]
    terminateRequested [ ^terminateRequested ]
    isTerminated [ ^state == #terminated ]
    isSuspended [ ^state == #suspended ]
    isReady [ ^state == #ready ]
    isRunning [ ^state == #running ]
    resume [ Processor resume: self ]
    suspend [ Processor suspend: self ]
    terminate [ Processor terminate: self ]
    yield [ Processor yield ]
    printOn: aStream [
        aStream nextPutAll: 'a Process(', (name ifNil: ['unnamed']) printString, ', priority ', priority printString, ', ', state, ')'
    ]
]

Object subclass: ProcessorScheduler [
    | ready active mainProcess |
    ProcessorScheduler class >> new [ ^self basicNew init ]
    init [
        ready := (1 to: 9) collect: [:i | OrderedCollection new].
        mainProcess := Process on: nil priority: 4.
        mainProcess name: 'main'; setState: #running.
        active := mainProcess
    ]
    idlePriority [ ^1 ]
    systemBackgroundPriority [ ^2 ]
    userBackgroundPriority [ ^3 ]
    userSchedulingPriority [ ^4 ]
    userInterruptPriority [ ^5 ]
    lowIOPriority [ ^6 ]
    highIOPriority [ ^7 ]
    timingPriority [ ^8 ]
    highestPriority [ ^9 ]
    activeProcess [ ^active ]
    activePriority [ ^active priority ]

    "put a new Process in a queue; a higher priority one runs at once"
    schedule: p [
        p setState: #ready.
        (ready at: p priority) addLast: p.
        p priority > active priority ifTrue: [self runPending: p priority]
    ]
    resume: p [
        p isSuspended ifFalse: [^self].
        p == active ifTrue: [^self].
        p block isNil ifTrue: [p setState: #running. ^self].
        self schedule: p
    ]
    suspend: p [
        p == active
            ifTrue: [
                p setState: #suspended.
                [p isSuspended] whileTrue: [self checkTerminate. self runOne ifFalse: [^self deadlock: 'suspend']].
                ^self].
        p isReady ifTrue: [(ready at: p priority) remove: p ifAbsent: [nil]].
        p setState: #suspended
    ]
    terminate: p [
        p isTerminated ifTrue: [^self].
        p == active ifTrue: [^ProcessTerminated new signal].
        p isReady ifTrue: [(ready at: p priority) remove: p ifAbsent: [nil]].
        p setTerminated
    ]
    terminateActive [ ^self terminate: active ]
    checkTerminate [ active terminateRequested ifTrue: [^ProcessTerminated new signal] ]

    "answers the best ready Process of at least the given priority, removing it"
    takeReady: minPriority [
        9 to: minPriority by: -1 do: [:i |
            (ready at: i) isEmpty ifFalse: [^(ready at: i) removeFirst]].
        ^nil
    ]
    hasReady [ ready do: [:q | q isEmpty ifFalse: [^true]]. ^false ]

    runProcess: p [
        | saved |
        saved := active.
        active := p.
        p setState: #running.
        [[p block value]
            on: ProcessTerminated do: [:e | e return: nil]
            on: Error do: [:e | Transcript showCr: 'Error in process ', (p name ifNil: ['unnamed']), ': ', e messageText]]
            ensure: [p setState: #terminated. active := saved]
    ]
    runOne [
        | p |
        p := self takeReady: 1.
        p isNil ifTrue: [^false].
        self runProcess: p.
        ^true
    ]
    runPending: minPriority [
        | p |
        [p := self takeReady: minPriority. p notNil] whileTrue: [self runProcess: p]
    ]
    "called at the end of every top-level statement"
    runPending [ self hasReady ifTrue: [self runPending: 1] ]
    yield [ self runPending: active priority ]
    sleep: ms [
        | until |
        until := Time millisecondClock + ms.
        [Time millisecondClock < until] whileTrue: [
            self checkTerminate.
            self runOne ifFalse: [
                Smalltalk system: 'sleep ', ((until - Time millisecondClock max: 0) / 1000.0) printString.
                ^self]]
    ]
    deadlock: what [
        ^self error: 'deadlock: ', what, ' with no other process able to run'
    ]
    "wait until the block answers true, running other processes meanwhile"
    waitUntil: aBlock for: what [
        [aBlock value] whileFalse: [
            self checkTerminate.
            self runOne ifFalse: [^self deadlock: what]]
    ]
]

Process extend [
    setTerminated [ state := #terminated. terminateRequested := true ]
]

Object subclass: Semaphore [
    | signals |
    Semaphore class >> new [ ^self basicNew setSignals: 0 ]
    Semaphore class >> forMutualExclusion [ ^self basicNew setSignals: 1 ]
    setSignals: n [ signals := n ]
    signals [ ^signals ]
    signal [ signals := signals + 1 ]
    wait [
        Processor waitUntil: [signals > 0] for: 'Semaphore wait'.
        signals := signals - 1
    ]
    critical: aBlock [
        self wait.
        ^aBlock ensure: [self signal]
    ]
    isEmpty [ ^signals = 0 ]
    printOn: aStream [ aStream nextPutAll: 'a Semaphore(', signals printString, ')' ]
]

Object subclass: SharedQueue [
    | items |
    SharedQueue class >> new [ ^self basicNew setItems ]
    setItems [ items := OrderedCollection new ]
    nextPut: x [ items addLast: x. ^x ]
    next [
        Processor waitUntil: [items notEmpty] for: 'SharedQueue next'.
        ^items removeFirst
    ]
    peek [ ^items isEmpty ifTrue: [nil] ifFalse: [items first] ]
    isEmpty [ ^items isEmpty ]
    size [ ^items size ]
    flush [ items := OrderedCollection new ]
]

Object subclass: Delay [
    | ms |
    Delay class >> forMilliseconds: n [ ^self basicNew setMs: n ]
    Delay class >> forSeconds: n [ ^self basicNew setMs: n * 1000 ]
    setMs: n [ ms := n ]
    wait [ Processor sleep: ms ]
    milliseconds [ ^ms ]
]

BlockClosure extend [
    newProcess [ ^Process on: self priority: Processor activePriority ]
    fork [ ^self forkAt: Processor activePriority ]
    forkAt: p [ | pr | pr := Process on: self priority: p. Processor schedule: pr. ^pr ]
    forkNamed: aString [ | pr | pr := Process on: self priority: Processor activePriority. pr name: aString. Processor schedule: pr. ^pr ]
    valueWithoutPreemption [ ^self value ]
]

Smalltalk at: #Processor put: ProcessorScheduler new.
