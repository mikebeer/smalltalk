| s |
s := 'Hello, World'.
s size printNl.
s asUppercase printNl. s asLowercase printNl. s reverse printNl. s reversed printNl.
(s at: 1) printNl. s first printNl. s last printNl.
(s copyFrom: 1 to: 5) printNl.
(s indexOf: $o) printNl.
(s occurrencesOf: $l) printNl.
(s includesSubstring: 'World') printNl.
(s startsWith: 'Hello') printNl. (s endsWith: 'World') printNl.
(s , '!') printNl.
(s copyReplaceAll: 'World' with: 'There') printNl.
s asSymbol printNl.
s subStrings printNl. (s subStrings: ', ') printNl.
s asBag size printNl. s asSet size printNl.
s displayString printNl. s printString printNl. s printString size printNl.
'it''s' printNl. 'it''s' displayNl. 'it''s' size printNl.
('abc' < 'abd') printNl. ('abc' = 'abc') printNl. ('abc' > 'abd') printNl.
('abc' max: 'abd') printNl.
#abc size printNl. (#abc , 'def') printNl. #abc asString printNl.
'  trim me  ' trimSeparators printNl.
'hello world foo' substrings first printNl.
('hello' collect: [:c | c asUppercase]) printNl.
('hello' select: [:c | c isVowel]) printNl.
('hello' detect: [:c | c > $h]) printNl.
'hello' asArray printNl. 'hello' asBag size printNl. 'olleh' asOrderedCollection printNl.
(String new: 3) size printNl.
(String new: 2 withAll: $z) printNl.
String new printNl.
('abc' includes: $b) printNl.
'42' asInteger printNl. 'x' asInteger printNl. '42' asNumber + 1.
('42' asNumber + 1) printNl.
42 printString printNl. 42 displayString printNl. 42 asString printNl. 3.5 printString printNl.
(42 printString: 2) printNl. (255 printString: 16) printNl. (255 radix: 16) printNl.
(5 printPaddedWith: $0 to: 3) printNl.
$a asInteger printNl. 97 asCharacter printNl. $a asUppercase printNl. $a isVowel printNl. $5 digitValue printNl.
(Character value: 65) printNl. Character nl printNl. Character space printNl. Character tab printNl.
$a < $b. ($a < $b) printNl.
'abc' hash = 'abc' hash.
('abc' hash = 'abc' hash) printNl.
('hello world' replaceAll: $o with: $0) printNl.
('hello' copyWith: $!) printNl.
('a:b' subStrings: ':') printNl.
('one two' , ' three') size printNl.
'abc' < 5.
('%1 is %2' bindWith: 'x' with: 3) displayNl.
('hello' at: 1 put: $j; yourself) printNl.
#sym at: 1 put: $x.
'tab	in' printNl.
(String with: $a with: $b) printNl.
('a-b-c' copyReplaceAll: '-' with: '--') printNl.
'hello' first: 2.
('hello' first: 2) printNl. ('hello' last: 3) printNl.
