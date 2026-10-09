"exception handling"
| r count |
r := [10/0] on: ZeroDivide do: [:e | e messageText displayNl. -1].
r printNl.
r := [10/0. 5] on: ZeroDivide do: [:e | e retryUsing: [99]].
r printNl.
count := 0.
r := [count := count + 1. count < 3 ifTrue: [Error signal: 'again']. count] on: Error do: [:e | e retry].
r printNl.
r := [Warning signal: 'warn'. 'resumed'] on: Warning do: [:e | e resume: nil].
r printNl.
r := [(Warning signal: 'warn') printNl. 'done'] on: Warning do: [:e | e resume: 42].
r printNl.
r := [[1/0] on: ZeroDivide do: [:e | e pass]] on: ZeroDivide do: [:e | 'outer caught' displayNl. 7].
r printNl.
r := [[1/0] on: ZeroDivide do: [:e | e signal]] on: ZeroDivide do: [:e | 'resignal caught' displayNl. 8].
r printNl.
r := [[Error signal: 'inner'] ensure: ['ensure ran' displayNl]] on: Error do: [:e | 'handler' displayNl. 9].
r printNl.
r := [[Error signal: 'inner'] ifCurtailed: ['curtailed' displayNl]] on: Error do: [:e | 10].
r printNl.
Error subclass: MyError [
    | code |
    code [ ^code ]
    code: c [ code := c ]
    messageText [ ^'my error code ', code printString ]
]
r := [MyError new code: 5; signal] on: MyError do: [:e | e messageText].
r displayNl.
r := [MyError new code: 6; signal] on: Error do: [:e | e class name].
r displayNl.
MyError new code: 7; signal.
'next' displayNl.
r := [nil foo] on: MessageNotUnderstood do: [:e | e messageText displayNl. e message selector printNl. e return: 3].
r printNl.
r := [#(1 2) at: 5] on: SystemExceptions.IndexOutOfRange do: [:e | e messageText].
r displayNl.
r := [Error signal] on: Error do: [:e | e messageText].
r printNl.
r := [Error new signal: 'abc'] on: Error do: [:e | e description].
r printNl.
r := [ZeroDivide new signal: 'custom'] on: ZeroDivide do: [:e | e messageText].
r printNl.
r := [1/0] on: ZeroDivide, MessageNotUnderstood do: [:e | 'set'].
r printNl.
r := [Error signal: 'a'. 1] on: Error do: [:e | e resume: 5].
'after bad resume' displayNl.
r := [self error: 'custom error'] on: Error do: [:e | e messageText].
r displayNl.
self error: 'unhandled'.
(Error new) signal: 'direct'.
'end' displayNl.
[^5] ensure: ['ensure at top' displayNl].
[Error signal: 'x'. 'resumed here' displayNl] on: Error do: [:e | e retryUsing: ['retryUsing' displayNl]].
