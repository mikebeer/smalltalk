"A bank account with custom exceptions"
Error subclass: InsufficientFunds [
    | needed |
    needed [ ^needed ]
    needed: n [ needed := n ]
    messageText [ ^'Insufficient funds: need ', needed printString, ' more' ]
]
Object subclass: Account [
    | balance |
    init [ balance := 0 ]
    Account class >> new [ ^super new init ]
    balance [ ^balance ]
    deposit: amt [
        amt <= 0 ifTrue: [^self error: 'Invalid amount'].
        balance := balance + amt
    ]
    withdraw: amt [
        amt > balance ifTrue: [^InsufficientFunds new needed: amt - balance; signal].
        balance := balance - amt
    ]
]
| acct |
acct := Account new.
acct deposit: 100.
acct withdraw: 30.
acct balance printNl.
[acct withdraw: 500] on: InsufficientFunds do: [:e |
    Transcript showCr: 'Caught: ', e messageText.
    e return: nil].
[acct deposit: -5] on: Error do: [:e | Transcript showCr: 'Error: ', e messageText].
[acct withdraw: 500] ensure: [Transcript showCr: 'done'].
acct balance printNl.
