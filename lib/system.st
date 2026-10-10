"system.st - files, directories, operating system access, CSV"

FileStream class extend [
    delete: path [ <primitive: fileDelete> ^false ]
    rename: old to: new [ <primitive: fileRename> ^false ]
    copy: old to: new [ <primitive: fileCopy> ^false ]
    size: path [ <primitive: fileSize> ^nil ]
    isFile: path [ ^(Directory kind: path) = 1 ]
    contentsOf: path ifAbsent: aBlock [
        | s |
        s := self contentsOf: path.
        ^s isNil ifTrue: [aBlock value] ifFalse: [s]
    ]
    lines: path [
        | s |
        s := self contentsOf: path.
        s isNil ifTrue: [^SystemExceptions.NotFound signalOn: path what: 'file' reason: 'could not open file ', path].
        ^s lines
    ]
    write: path contents: text [
        | f |
        f := self write: path.
        f nextPutAll: text.
        f close.
        ^text size
    ]
    append: path contents: text [
        | f |
        f := self append: path.
        f nextPutAll: text.
        f close.
        ^text size
    ]
]

Object subclass: Directory [
    Directory class >> kind: path [ <primitive: fileKind> ^0 ]
    Directory class >> exists: path [ ^(self kind: path) = 2 ]
    Directory class >> create: path [ <primitive: dirCreate> ^false ]
    Directory class >> remove: path [ <primitive: dirRemove> ^false ]
    Directory class >> primList: path [ <primitive: dirList> ^nil ]
    Directory class >> current [ <primitive: cwd> ]
    Directory class >> current: path [ <primitive: chdir> ^false ]
    Directory class >> entries: path [
        | a |
        a := self primList: path.
        a isNil ifTrue: [^SystemExceptions.NotFound signalOn: path what: 'directory' reason: 'could not read directory ', path].
        ^a asSortedCollection asArray
    ]
    Directory class >> fileNames: path [
        ^(self entries: path) select: [:n | (self kind: path, '/', n) = 1]
    ]
    Directory class >> directoryNames: path [
        ^(self entries: path) select: [:n | (self kind: path, '/', n) = 2]
    ]
]

SystemDictionary extend [
    primArgCount [ <primitive: argCount> ]
    primArgAt: i [ <primitive: argAt> ]
    arguments [
        | r |
        r := Array new: self primArgCount.
        1 to: r size do: [:i | r at: i put: (self primArgAt: i)].
        ^r
    ]
    systemOutput: command [ <primitive: sysCmd> ^'' ]
    system: command [
        | out |
        out := self systemOutput: command.
        out isEmpty ifFalse: [Transcript showCr: out].
        ^self lastExitCode
    ]
    lastExitCode [ <primitive: sysRc> ]
    systemError [ <primitive: sysErr> ]
    currentDirectory [ ^Directory current ]
]

Object subclass: CSV [
    CSV class >> parse: text [
        | rows row field i c n inQuotes any |
        rows := OrderedCollection new.
        row := OrderedCollection new.
        field := WriteStream on: String new.
        inQuotes := false. any := false.
        i := 1. n := text size.
        [i <= n] whileTrue: [
            c := text at: i.
            inQuotes
                ifTrue: [
                    c = $"
                        ifTrue: [
                            (i < n and: [(text at: i + 1) = $"])
                                ifTrue: [field nextPut: $". i := i + 1]
                                ifFalse: [inQuotes := false]]
                        ifFalse: [field nextPut: c]]
                ifFalse: [
                    c = $" ifTrue: [inQuotes := true. any := true] ifFalse: [
                    c = $, ifTrue: [row add: field contents. field := WriteStream on: String new. any := true] ifFalse: [
                    c value = 13 ifFalse: [
                        c value = 10
                            ifTrue: [
                                (any or: [row notEmpty or: [field contents notEmpty]])
                                    ifTrue: [row add: field contents. rows add: row asArray].
                                row := OrderedCollection new.
                                field := WriteStream on: String new.
                                any := false]
                            ifFalse: [field nextPut: c. any := true]]]]].
            i := i + 1].
        (any or: [row notEmpty or: [field contents notEmpty]])
            ifTrue: [row add: field contents. rows add: row asArray].
        ^rows asArray
    ]
    CSV class >> parseFile: path [ ^self parse: (FileStream contentsOf: path ifAbsent: [^SystemExceptions.NotFound signalOn: path what: 'file' reason: 'could not open file ', path]) ]
    CSV class >> parseWithHeader: text [
        | rows header |
        rows := self parse: text.
        rows isEmpty ifTrue: [^Array new].
        header := rows first.
        ^(rows copyFrom: 2 to: rows size) collect: [:r |
            | d |
            d := Dictionary new.
            1 to: header size do: [:k | d at: (header at: k) put: (k <= r size ifTrue: [r at: k] ifFalse: [''])].
            d]
    ]
    CSV class >> quote: aField [
        | s |
        s := aField displayString.
        ((s includes: $,) or: [(s includes: $") or: [(s includes: (Character value: 10)) or: [(s includes: (Character value: 13)) or: [s notEmpty and: [s first isSeparator or: [s last isSeparator]]]]]])
            ifFalse: [^s].
        ^'"', (s copyReplaceAll: '"' with: '""'), '"'
    ]
    CSV class >> format: rows [
        | ws |
        ws := WriteStream on: String new.
        rows do: [:r |
            | first |
            first := true.
            r do: [:f | first ifFalse: [ws nextPut: $,]. first := false. ws nextPutAll: (self quote: f)].
            ws nl].
        ^ws contents
    ]
    CSV class >> write: rows to: path [ ^FileStream write: path contents: (self format: rows) ]
]

String extend [
    asCsvRows [ ^CSV parse: self ]
]
Collection extend [
    asCsv [ ^CSV format: self ]
]

"evaluating and compiling source at run time"
SystemDictionary extend [
    evaluate: aString [ <primitive: evaluate> ^nil ]
    fileIn: aString [ <primitive: fileInString> ^nil ]
]

Behavior extend [
    compile: aString [
        | first rest src i |
        i := aString indexOf: $[.
        (i > 0 and: [(aString copyFrom: 1 to: i - 1) allSatisfy: [:c | c ~= $^ and: [c ~= $|]]])
            ifTrue: [src := aString]
            ifFalse: [
                i := (aString indexOf: $^).
                (i = 0 or: [(aString indexOf: $|) between: 1 and: i - 1]) ifTrue: [i := aString indexOf: $|].
                i = 0 ifTrue: [i := aString size + 1].
                src := (aString copyFrom: 1 to: i - 1), ' [ ', (aString copyFrom: i to: aString size), ' ]'].
        self fileInSource: self name, ' extend [ ', src, ' ]'.
        ^self
    ]
    evaluate: aString [ ^Smalltalk evaluate: aString ]
    fileInSource: aString [ <primitive: fileInString> ^nil ]
]

"a snapshot of the stack, answered by thisContext"
Object subclass: Context [
    | receiver selector methodClass sender |
    receiver [ ^receiver ]
    selector [ ^selector ]
    methodClass [ ^methodClass ]
    sender [ ^sender ]
    parentContext [ ^sender ]
    isTopLevel [ ^sender isNil ]
    depth [ | n c | n := 0. c := self. [c notNil] whileTrue: [n := n + 1. c := c sender]. ^n ]
    printOn: aStream [
        methodClass isNil ifTrue: [^aStream nextPutAll: 'UndefinedObject>>', selector].
        receiver class == methodClass
            ifTrue: [aStream nextPutAll: methodClass name, '>>', selector]
            ifFalse: [aStream nextPutAll: receiver class name, '(', methodClass name, ')>>', selector]
    ]
    backtrace [
        | c |
        c := self.
        [c notNil] whileTrue: [Transcript showCr: c printString. c := c sender]
    ]
    backtraceString [
        | ws c |
        ws := WriteStream on: String new.
        c := self.
        [c notNil] whileTrue: [ws nextPutAll: c printString; nl. c := c sender].
        ^ws contents
    ]
]
