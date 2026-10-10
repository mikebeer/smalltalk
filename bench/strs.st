| t ws str |
t := Time millisecondClock.
ws := WriteStream on: String new.
1 to: 3000 do: [:i | ws nextPutAll: i printString; space].
ws contents size printNl.
str := ''.
1 to: 1500 do: [:i | str := str , 'ab'].
str size printNl.
('hello world foo bar' substrings: ' ') size printNl.
(1 to: 3000) inject: 0 into: [:a :i | a + i printString size].
(Time millisecondClock - t) displayNl.
