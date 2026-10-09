"Towers of Hanoi with blocks and recursion"
| moves hanoi |
moves := 0.
hanoi := nil.
hanoi := [:n :from :to :via |
    n > 0 ifTrue: [
        hanoi value: n - 1 value: from value: via value: to.
        moves := moves + 1.
        n = 3 ifTrue: [Transcript showCr: 'move disc 3 from ', from, ' to ', to].
        hanoi value: n - 1 value: via value: to value: from]].
hanoi value: 5 value: 'A' value: 'C' value: 'B'.
moves printNl.
