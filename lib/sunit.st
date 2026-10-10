"sunit.st - a small SUnit: TestCase, TestSuite, TestResult"

Error subclass: TestFailure [
]

Object subclass: TestResult [
    | passed failures errors |
    TestResult class >> new [ ^self basicNew init ]
    init [ passed := OrderedCollection new. failures := OrderedCollection new. errors := OrderedCollection new ]
    passed [ ^passed ]
    failures [ ^failures ]
    errors [ ^errors ]
    runCount [ ^passed size + failures size + errors size ]
    passedCount [ ^passed size ]
    failureCount [ ^failures size ]
    errorCount [ ^errors size ]
    hasPassed [ ^failures isEmpty and: [errors isEmpty] ]
    addPass: aTest [ passed add: aTest ]
    addFailure: aTest [ failures add: aTest ]
    addError: aTest [ errors add: aTest ]
    printOn: aStream [
        aStream nextPutAll: self runCount printString, ' run, ', passed size printString, ' passed, ',
            failures size printString, ' failed, ', errors size printString, ' errors'
    ]
]

Object subclass: TestSuite [
    | tests name |
    TestSuite class >> new [ ^self basicNew init ]
    TestSuite class >> named: aString [ ^self new name: aString; yourself ]
    init [ tests := OrderedCollection new. name := 'suite' ]
    name [ ^name ]
    name: aString [ name := aString ]
    tests [ ^tests ]
    addTest: aTest [ tests add: aTest ]
    addTests: aCollection [ aCollection do: [:t | tests add: t] ]
    size [ ^tests inject: 0 into: [:a :t | a + t size] ]
    run [ | r | r := TestResult new. self runOn: r. ^r ]
    runOn: aResult [ tests do: [:t | t runOn: aResult] ]
]

Object subclass: TestCase [
    | selector failureText |
    TestCase class >> selector: aSymbol [ ^self basicNew setSelector: aSymbol ]
    TestCase class >> isAbstract [ ^self == TestCase ]
    "the names of the test methods of this class and its superclasses below TestCase"
    TestCase class >> testSelectors [
        | c names |
        names := OrderedCollection new.
        c := self.
        [c notNil and: [c ~~ TestCase]] whileTrue: [
            c selectors asSortedCollection do: [:s | ((s startsWith: 'test') and: [(names includes: s) not]) ifTrue: [names add: s]].
            c := c superclass].
        ^names asSortedCollection asArray
    ]
    TestCase class >> suite [
        | s |
        s := TestSuite named: self name.
        self testSelectors do: [:sel | s addTest: (self selector: sel)].
        ^s
    ]
    TestCase class >> buildSuite [ ^self suite ]
    TestCase class >> run [ ^self suite run ]
    TestCase class >> run: aSymbol [ ^(self selector: aSymbol) run ]
    "run the tests and show a report on the Transcript"
    TestCase class >> runAndReport [
        | r |
        r := self run.
        Transcript showCr: self name, ': ', r printString.
        r failures do: [:t | Transcript showCr: '  FAIL ', t printString, ': ', t failureText].
        r errors do: [:t | Transcript showCr: '  ERROR ', t printString, ': ', t failureText].
        ^r
    ]

    setSelector: aSymbol [ selector := aSymbol ]
    selector [ ^selector ]
    failureText [ ^failureText ]
    size [ ^1 ]
    name [ ^self class name, '>>', selector ]
    printOn: aStream [ aStream nextPutAll: self class name, '>>#', selector ]
    setUp [ ]
    tearDown [ ]

    run [ | r | r := TestResult new. self runOn: r. ^r ]
    runOn: aResult [
        | ok |
        ok := false.
        [[self setUp. self perform: selector. ok := true]
            ensure: [self tearDown]]
            on: TestFailure do: [:e | failureText := e messageText. aResult addFailure: self. ^self]
            on: Error do: [:e | failureText := e messageText. aResult addError: self. ^self].
        ok ifTrue: [aResult addPass: self]
    ]

    assert: aBoolean [ aBoolean ifFalse: [self fail: 'Assertion failed'] ]
    assert: aBoolean description: aString [ aBoolean ifFalse: [self fail: aString] ]
    deny: aBoolean [ aBoolean ifTrue: [self fail: 'Denial failed'] ]
    deny: aBoolean description: aString [ aBoolean ifTrue: [self fail: aString] ]
    assert: actual equals: expected [
        actual = expected ifFalse: [self fail: 'Expected ', expected printString, ' but got ', actual printString]
    ]
    assert: actual closeTo: expected [
        (actual - expected) abs < 0.0001 ifFalse: [self fail: 'Expected about ', expected printString, ' but got ', actual printString]
    ]
    fail [ self fail: 'Test failed' ]
    fail: aString [ ^TestFailure new signal: aString ]
    should: aBlock [ self assert: aBlock value ]
    shouldnt: aBlock [ self deny: aBlock value ]
    should: aBlock raise: anExceptionClass [
        | raised |
        raised := false.
        aBlock on: anExceptionClass do: [:e | raised := true].
        raised ifFalse: [self fail: 'Expected ', anExceptionClass name, ' was not raised']
    ]
    shouldnt: aBlock raise: anExceptionClass [
        | raised |
        raised := false.
        aBlock on: anExceptionClass do: [:e | raised := true].
        raised ifTrue: [self fail: anExceptionClass name, ' was raised']
    ]
    should: aBlock raise: anExceptionClass description: aString [
        | raised |
        raised := false.
        aBlock on: anExceptionClass do: [:e | raised := true].
        raised ifFalse: [self fail: aString]
    ]
]
