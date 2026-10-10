('hello world' matchesRegex: 'h.*d') printNl.
('abc123' matchesRegex: '[a-z]+\d+') printNl.
('abc123x' matchesRegex: '[a-z]+\d+') printNl.
('foo 12 bar 345' allRegexMatches: '\d+') printNl.
(('2026-10-10' searchRegex: '(\d+)-(\d+)-(\d+)') group: 2) printNl.
('2026-10-10' searchRegex: '(\d+)-(\d+)-(\d+)') printNl.
('hello world' copyReplacingRegex: 'o' with: '0') printNl.
('John Smith' copyReplacingRegex: '(\w+) (\w+)' with: '\2, \1') printNl.
('a1b22c' splitOnRegex: '\d+') printNl.
('I like dogs and cats' allRegexMatches: 'cat|dog') printNl.
('color colour' allRegexMatches: 'colou?r') printNl.
('aa aaa aaaa' allRegexMatches: 'a{2,3}') printNl.
('abc' matchesRegex: '^abc$') printNl.
('<a><b>' regexMatch: '<.+?>') printNl.
('<a><b>' regexMatch: '<.+>') printNl.
('ababab' regexMatch: '(ab)+') printNl.
('a cat sat, concat' allRegexMatches: '\bcat\b') printNl.
('x@y.com z@w.org' allRegexMatches: '[\w.]+@[\w.]+\.[a-z]+') printNl.
('abc' matchesRegex: '[^x]+') printNl.
('a-b' matchesRegex: '[a\-b]+') printNl.
('  trim me  ' copyReplacingRegex: '^\s+|\s+$' with: '') printNl.
('hello' occurrencesOfRegex: 'l') printNl.
('' matchesRegex: 'a*') printNl.
('abc' copyReplacingRegex: 'x*' with: '-') printNl.
('aaa' matchesRegex: '(a*)*b') printNl.
('(x)' matchesRegex: '\(x\)') printNl.
('ab' matchesRegex: '(?:a)(b)') printNl.
('ab' matchesRegex: '(a') printNl.
('a-b' matchesRegex: '[a\-b]+') printNl.
('x1y' matchesRegex: '[a-c0-9x-z]+') printNl.
('q' matchesRegex: '[a-c0-9x-z]') printNl.
('hello world' allRegexMatches: '[aeiou]') size printNl.
