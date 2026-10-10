'2026-10-10' asDate printNl.
'2026/1/5' asDate printNl.
'14:05:09' asTime printNl.
'14:05' asTime printNl.
'2026-10-10T14:05:09' asDateTime printNl.
('2026-10-10' asDate formatted: 'day, dd mon yyyy') displayNl.
('2026-10-10' asDate formatted: 'month dd, yy') displayNl.
('2026-10-10' asDate formatted: 'dd/mm/yyyy') displayNl.
('14:05:09' asTime formatted: 'hh.mm') displayNl.
'bad' asDate.
(Smalltalk getenv: 'HOME') isString printNl.
(Smalltalk getenv: 'NO_SUCH_VAR_X') printNl.
Random seed: 42.
| a b |
a := Random new next.
Random seed: 42.
b := Random new next.
(a = b) printNl.
Smalltalk os printNl.
