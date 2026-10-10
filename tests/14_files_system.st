-1 sqrt printNl.
-1 sqrt isNaN printNl.
Float infinity printNl.
Float negativeInfinity printNl.
Float nan printNl.
Float infinity isInfinite printNl.
3.5 isInfinite printNl.
0 ln printNl.
-2 ln isNaN printNl.
Float infinity truncated.
(Set withAll: #(5 3 9 1)) printNl.
(Bag withAll: #(3 1 3 2)) printNl.
(Dictionary new at: #b put: 2; at: #a put: 1; at: #c put: 3; yourself) printNl.
(Set withAll: #('pear' 'apple' 'fig')) displayNl.
(Set withAll: #(1 $a 'x')) size printNl.
LinkedList new first.
#() first.
OrderedCollection new removeFirst.
(Time millisecondClock > 1700000000000) printNl.
(Time millisecondsToRun: [1000 timesRepeat: [2 + 3]]) isInteger printNl.
(Directory create: '/tmp/stdir1') printNl.
(Directory exists: '/tmp/stdir1') printNl.
(FileStream write: '/tmp/stdir1/b.txt' contents: 'hello') printNl.
(FileStream write: '/tmp/stdir1/a.txt' contents: 'x,y') printNl.
(Directory create: '/tmp/stdir1/sub') printNl.
(Directory entries: '/tmp/stdir1') printNl.
(Directory fileNames: '/tmp/stdir1') printNl.
(Directory directoryNames: '/tmp/stdir1') printNl.
(FileStream size: '/tmp/stdir1/b.txt') printNl.
(FileStream copy: '/tmp/stdir1/b.txt' to: '/tmp/stdir1/c.txt') printNl.
(FileStream rename: '/tmp/stdir1/c.txt' to: '/tmp/stdir1/d.txt') printNl.
(FileStream exists: '/tmp/stdir1/c.txt') printNl.
(FileStream contentsOf: '/tmp/stdir1/d.txt') printNl.
(FileStream delete: '/tmp/stdir1/d.txt') printNl.
(FileStream delete: '/tmp/stdir1/d.txt') printNl.
(FileStream isFile: '/tmp/stdir1/b.txt') printNl.
Directory entries: '/tmp/nope_dir'.
(Smalltalk systemOutput: 'echo hi; echo there') printNl.
Smalltalk lastExitCode printNl.
(Smalltalk system: 'echo run') printNl.
(Smalltalk system: 'exit 3') printNl.
(FileStream delete: '/tmp/stdir1/a.txt') printNl.
(FileStream delete: '/tmp/stdir1/b.txt') printNl.
(Directory remove: '/tmp/stdir1/sub') printNl.
(Directory remove: '/tmp/stdir1') printNl.
(Directory exists: '/tmp/stdir1') printNl.
(CSV parse: 'a,b,c
1,"x, y",3
"he said ""hi""",,z') printNl.
(CSV parse: 'a,b' , (String with: (Character value: 13) with: (Character value: 10)), '1,2') printNl.
(CSV parseWithHeader: 'name,age
Ann,30
Bob,25') printNl.
(CSV format: #(#('a' 'b c') #('x,y' 'say "hi"') #(1 2.5 nil))) displayString displayNl.
(CSV write: #(#(1 2) #(3 4)) to: '/tmp/st_csv_t.csv') printNl.
(CSV parseFile: '/tmp/st_csv_t.csv') printNl.
(FileStream delete: '/tmp/st_csv_t.csv') printNl.
CSV parseFile: '/tmp/none.csv'.
'x,y
1,2' asCsvRows printNl.
CSV parse: '' .
(CSV parse: '') printNl.
-1 sqrt printNl.
Float infinity printNl.
(Smalltalk system: 'echo done') printNl.
(FileStream write: '/tmp/ff1.txt' contents: 'hello') printNl.
(FileStream contentsOf: '/tmp/ff1.txt') printNl.
(FileStream size: '/tmp/ff1.txt') printNl.
(FileStream write: '/tmp/ff1.txt' contents: 'a
b
') printNl.
(FileStream contentsOf: '/tmp/ff1.txt') printNl.
(FileStream lines: '/tmp/ff1.txt') printNl.
(FileStream write: '/tmp/ff1.txt' contents: '') printNl.
(FileStream contentsOf: '/tmp/ff1.txt') printNl.
(FileStream size: '/tmp/ff1.txt') printNl.
FileStream delete: '/tmp/ff1.txt'.
| rw ws |
rw := ReadWriteStream on: String new.
rw nextPutAll: 'hello world'.
rw reset.
(rw next: 5) printNl.
rw upToEnd printNl.
(ReadWriteStream with: 'abc') contents printNl.
ws := WriteStream on: String new.
ws << 'a' << 1 << $c << #sym << 2.5 << nil; nl.
ws contents printNl.
(255 printString: 16 paddedWith: $0 to: 6) printNl.
(5 printPaddedWith: $0 to: 3) printNl.
(5 printString: 2) printNl.
(ws contents size) printNl.
ws := WriteStream with: 'abc'.
ws nextPutAll: 'def'.
ws contents printNl.
(ReadStream on: 'abcdef' from: 2 to: 4) upToEnd printNl.
