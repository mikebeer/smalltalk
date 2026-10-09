"Streams and strings"
| ws rs |
ws := WriteStream on: String new.
#('alpha' 'beta' 'gamma') do: [:w | ws nextPutAll: w] separatedBy: [ws nextPutAll: ', '].
ws contents displayNl.
ws := WriteStream on: Array new.
1 to: 5 do: [:i | ws nextPut: i * i].
ws contents printNl.
rs := ReadStream on: 'one two  three'.
[rs atEnd] whileFalse: [(rs upTo: $ ) displayNl].
('hello world' select: [:c | c isVowel]) displayNl.
'hello' reverse displayNl.
('a,b,c' subStrings: ',') printNl.
(String new: 3 withAll: $x) displayNl.
