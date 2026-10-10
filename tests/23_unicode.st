"Unicode via the CREXX VM"
| s b |
s := 'Gràdh 中 ☺'.
s size printNl.
(s at: 7) value printNl.
(Character value: 16r1F600) value printNl.
(Character value: 20013) isLetter printNl.
$é asUppercase value printNl.
$É asLowercase value printNl.
s asUppercase printNl.
'Straße' asUppercase printNl.
'Straße' asCasefold printNl.
('Straße' sameAs: 'STRASSE') printNl.
s reverse printNl.
b := s utf8Bytes.
b size printNl.
b printNl.
(String fromUtf8Bytes: b) = s.
(String fromUtf8Bytes: b) printNl.
('e', (Character value: 769) asString) asNFC size printNl.
('e', (Character value: 769) asString) size printNl.
'Ünïcode' graphemeSize printNl.
(Character value: 55296) printNl.
