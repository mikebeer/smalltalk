"SUnit-style tests"
TestCase subclass: StackTest [
    | s |
    setUp [ s := OrderedCollection new. s add: 1; add: 2 ]
    tearDown [ Transcript showCr: 'tearDown ', selector ]
    testSize [ self assert: s size equals: 2 ]
    testAdd [ s add: 3. self assert: s last equals: 3. self assert: s size = 3 ]
    testFailing [ self assert: s size equals: 5 ]
    testError [ ^nil foo ]
    testRaise [ self should: [1/0] raise: ZeroDivide ]
    testRaiseMissing [ self should: [1+1] raise: ZeroDivide ]
    testDeny [ self deny: s isEmpty ]
    helper [ ^1 ]
]
StackTest subclass: SubTest [
    testExtra [ self assert: true ]
]
StackTest runAndReport.
StackTest suite size printNl.
(StackTest run: #testAdd) printNl.
SubTest suite size printNl.
SubTest run printNl.
(StackTest selector: #testFailing) run failures size printNl.
| suite r |
suite := TestSuite named: 'all'.
suite addTest: (StackTest selector: #testSize); addTest: (StackTest selector: #testDeny).
r := suite run.
r printNl. r hasPassed printNl.
