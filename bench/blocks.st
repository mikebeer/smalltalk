| t sum blk |
t := Time millisecondClock.
sum := 0.
blk := [:x | x * 2 + 1].
1 to: 200000 do: [:i | sum := sum + (blk value: i)].
sum printNl.
sum := 0.
#(1 2 3 4 5 6 7 8 9 10) do: [:e | 1 to: 10000 do: [:k | sum := sum + (e * k)]].
sum printNl.
(Time millisecondClock - t) displayNl.
