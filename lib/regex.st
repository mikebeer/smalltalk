"regex.st - a backtracking regular expression engine"

Object subclass: RegexMatch [
    | text start end groups |
    RegexMatch class >> text: t start: s end: e groups: g [ ^self basicNew setText: t start: s end: e groups: g ]
    setText: t start: s end: e groups: g [ text := t. start := s. end := e. groups := g ]
    start [ ^start ]
    stop [ ^end ]
    matched [ ^text copyFrom: start to: end ]
    groupCount [ ^groups size ]
    group: n [
        | g |
        n = 0 ifTrue: [^self matched].
        g := groups at: n.
        ^g isNil ifTrue: [nil] ifFalse: [text copyFrom: (g at: 1) to: (g at: 2) - 1]
    ]
    printOn: aStream [ aStream nextPutAll: 'RegexMatch('; print: self matched; nextPut: $) ]
]

Object subclass: Regex [
    | pat pos ngroups root text caps endIdx |
    Regex class >> on: aString [ ^self basicNew parse: aString ]

    parse: aString [
        pat := aString. pos := 1. ngroups := 0.
        root := self parseAlt.
        pos <= pat size ifTrue: [^self error: 'unbalanced ) in regex'].
        ^self
    ]
    peekc [ ^pos <= pat size ifTrue: [pat at: pos] ifFalse: [nil] ]
    parseAlt [
        | alts |
        alts := OrderedCollection new.
        alts add: self parseSeq.
        [self peekc = $|] whileTrue: [pos := pos + 1. alts add: self parseSeq].
        ^alts size = 1 ifTrue: [alts first] ifFalse: [Array with: #alt with: alts asArray]
    ]
    parseSeq [
        | items c |
        items := OrderedCollection new.
        [c := self peekc. c notNil and: [c ~= $| and: [c ~= $)]]] whileTrue: [items add: self parseRep].
        ^Array with: #seq with: items asArray
    ]
    parseRep [
        | a c mn mx lazy b |
        a := self parseAtom.
        c := self peekc.
        mn := nil.
        c = $* ifTrue: [mn := 0. mx := 1000000. pos := pos + 1].
        c = $+ ifTrue: [mn := 1. mx := 1000000. pos := pos + 1].
        c = $? ifTrue: [mn := 0. mx := 1. pos := pos + 1].
        c = ${ ifTrue: [
            b := self parseBraces.
            b notNil ifTrue: [mn := b at: 1. mx := b at: 2]].
        mn isNil ifTrue: [^a].
        lazy := self peekc = $?.
        lazy ifTrue: [pos := pos + 1].
        ^Array with: #rep with: a with: mn with: mx with: lazy
    ]
    parseBraces [
        | p n m c |
        p := pos + 1.
        n := 0. m := nil.
        (p <= pat size and: [(pat at: p) isDigit]) ifFalse: [^nil].
        [p <= pat size and: [(pat at: p) isDigit]] whileTrue: [n := n * 10 + (pat at: p) digitValue. p := p + 1].
        p > pat size ifTrue: [^nil].
        c := pat at: p.
        c = $} ifTrue: [pos := p + 1. ^Array with: n with: n].
        c = $, ifFalse: [^nil].
        p := p + 1.
        (p <= pat size and: [(pat at: p) = $}]) ifTrue: [pos := p + 1. ^Array with: n with: 1000000].
        m := 0.
        (p <= pat size and: [(pat at: p) isDigit]) ifFalse: [^nil].
        [p <= pat size and: [(pat at: p) isDigit]] whileTrue: [m := m * 10 + (pat at: p) digitValue. p := p + 1].
        (p <= pat size and: [(pat at: p) = $}]) ifFalse: [^nil].
        pos := p + 1.
        ^Array with: n with: m
    ]
    parseAtom [
        | c idx n |
        c := pat at: pos.
        pos := pos + 1.
        c = $( ifTrue: [
            idx := 0.
            (pos + 1 <= pat size and: [(pat at: pos) = $? and: [(pat at: pos + 1) = $:]])
                ifTrue: [pos := pos + 2]
                ifFalse: [ngroups := ngroups + 1. idx := ngroups].
            n := self parseAlt.
            self peekc = $) ifFalse: [^self error: 'missing ) in regex'].
            pos := pos + 1.
            ^idx = 0 ifTrue: [n] ifFalse: [Array with: #group with: idx with: n]].
        c = $. ifTrue: [^Array with: #pred with: [:x | x value ~= 10]].
        c = $^ ifTrue: [^Array with: #bol].
        c = $$ ifTrue: [^Array with: #eol].
        c = $[ ifTrue: [^self parseClass].
        c = $\ ifTrue: [^self parseEscape].
        ^Array with: #pred with: [:x | x = c]
    ]
    predFor: e [
        e = $d ifTrue: [^[:x | x isDigit]].
        e = $D ifTrue: [^[:x | x isDigit not]].
        e = $w ifTrue: [^[:x | x isAlphaNumeric or: [x = $_]]].
        e = $W ifTrue: [^[:x | (x isAlphaNumeric or: [x = $_]) not]].
        e = $s ifTrue: [^[:x | x isSeparator]].
        e = $S ifTrue: [^[:x | x isSeparator not]].
        ^nil
    ]
    escChar: e [
        e = $n ifTrue: [^Character value: 10].
        e = $t ifTrue: [^Character value: 9].
        e = $r ifTrue: [^Character value: 13].
        ^e
    ]
    parseEscape [
        | e p ch |
        pos > pat size ifTrue: [^self error: 'trailing \ in regex'].
        e := pat at: pos.
        pos := pos + 1.
        e = $b ifTrue: [^Array with: #wordb].
        p := self predFor: e.
        p notNil ifTrue: [^Array with: #pred with: p].
        ch := self escChar: e.
        ^Array with: #pred with: [:x | x = ch]
    ]
    parseClass [
        | neg preds c first p hi lo done e |
        neg := false.
        self peekc = $^ ifTrue: [neg := true. pos := pos + 1].
        preds := OrderedCollection new.
        first := true.
        done := false.
        [done] whileFalse: [
            pos > pat size ifTrue: [^self error: 'missing ] in regex'].
            c := pat at: pos.
            pos := pos + 1.
            (c = $] and: [first not])
                ifTrue: [done := true]
                ifFalse: [
                    first := false.
                    c = $\
                        ifTrue: [
                            e := pat at: pos.
                            pos := pos + 1.
                            p := self predFor: e.
                            p notNil
                                ifTrue: [preds add: p]
                                ifFalse: [preds add: (self litPred: (self escChar: e))]]
                        ifFalse: [
                            ((self peekc = $-) and: [pos + 1 <= pat size and: [(pat at: pos + 1) ~= $]]])
                                ifTrue: [
                                    lo := c. hi := pat at: pos + 1.
                                    pos := pos + 2.
                                    preds add: (self rangePred: lo to: hi)]
                                ifFalse: [preds add: (self litPred: c)]]]].
        ^Array with: #pred with: (self classPred: preds negated: neg)
    ]

    litPred: c [ ^[:x | x = c] ]
    rangePred: lo to: hi [ ^[:x | x >= lo and: [x <= hi]] ]
    classPred: preds negated: neg [ ^[:x | (preds anySatisfy: [:q | q value: x]) ~= neg] ]
    isWordChar: c [ ^c isAlphaNumeric or: [c = $_] ]

    matchNode: n at: i k: k [
        | tag a b |
        tag := n at: 1.
        tag == #pred ifTrue: [^(i <= text size and: [(n at: 2) value: (text at: i)]) and: [k value: i + 1]].
        tag == #seq ifTrue: [^self matchSeq: (n at: 2) index: 1 at: i k: k].
        tag == #rep ifTrue: [^self matchRep: (n at: 2) min: (n at: 3) max: (n at: 4) lazy: (n at: 5) count: 0 at: i k: k].
        tag == #group ifTrue: [^self matchGroup: n at: i k: k].
        tag == #alt ifTrue: [
            (n at: 2) do: [:alt | (self matchNode: alt at: i k: k) ifTrue: [^true]].
            ^false].
        tag == #bol ifTrue: [^i = 1 and: [k value: i]].
        tag == #eol ifTrue: [^i = (text size + 1) and: [k value: i]].
        tag == #wordb ifTrue: [
            a := i > 1 and: [self isWordChar: (text at: i - 1)].
            b := i <= text size and: [self isWordChar: (text at: i)].
            ^a ~= b and: [k value: i]].
        ^self error: 'bad regex node'
    ]
    matchSeq: list index: j at: i k: k [
        j > list size ifTrue: [^k value: i].
        ^self matchNode: (list at: j) at: i k: [:e | self matchSeq: list index: j + 1 at: e k: k]
    ]
    matchGroup: n at: i k: k [
        | idx |
        idx := n at: 2.
        ^self matchNode: (n at: 3) at: i k: [:e | self groupDone: idx from: i to: e k: k]
    ]
    groupDone: idx from: i to: e k: k [
        | save |
        save := caps at: idx.
        caps at: idx put: (Array with: i with: e).
        (k value: e) ifTrue: [^true].
        caps at: idx put: save.
        ^false
    ]
    matchRep: nd min: mn max: mx lazy: lz count: c at: i k: k [
        lz ifTrue: [(c >= mn and: [k value: i]) ifTrue: [^true]].
        c < mx ifTrue: [
            (self matchNode: nd at: i k: [:e |
                (e = i and: [c >= mn])
                    ifTrue: [false]
                    ifFalse: [self matchRep: nd min: mn max: mx lazy: lz count: c + 1 at: e k: k]])
                ifTrue: [^true]].
        lz ifTrue: [^false].
        ^c >= mn and: [k value: i]
    ]

    searchIn: aString from: start [
        text := aString.
        start to: text size + 1 do: [:s |
            caps := Array new: ngroups withAll: nil.
            endIdx := nil.
            (self matchNode: root at: s k: [:e | endIdx := e. true])
                ifTrue: [^RegexMatch text: text start: s end: endIdx - 1 groups: caps copy]].
        ^nil
    ]
    matchesWhole: aString [
        text := aString.
        caps := Array new: ngroups withAll: nil.
        ^self matchNode: root at: 1 k: [:e | e = (text size + 1)]
    ]
]

String extend [
    searchRegex: p [ ^(Regex on: p) searchIn: self from: 1 ]
    matchesRegex: p [ ^(Regex on: p) matchesWhole: self ]
    regexMatch: p [ | m | m := self searchRegex: p. ^m isNil ifTrue: [nil] ifFalse: [m matched] ]
    allRegexMatchObjects: p [
        | rx out m at |
        rx := Regex on: p.
        out := OrderedCollection new.
        at := 1.
        [at <= (self size + 1) and: [(m := rx searchIn: self from: at) notNil]] whileTrue: [
            out add: m.
            at := m stop >= m start ifTrue: [m stop + 1] ifFalse: [m start + 1]].
        ^out asArray
    ]
    allRegexMatches: p [ ^(self allRegexMatchObjects: p) collect: [:m | m matched] ]
    occurrencesOfRegex: p [ ^(self allRegexMatchObjects: p) size ]
    expandRegexReplacement: rep with: m [
        | ws i c |
        ws := WriteStream on: String new.
        i := 1.
        [i <= rep size] whileTrue: [
            c := rep at: i.
            (c = $\ and: [i < rep size and: [(rep at: i + 1) isDigit]])
                ifTrue: [ | g | g := m group: (rep at: i + 1) digitValue. g notNil ifTrue: [ws nextPutAll: g]. i := i + 2]
                ifFalse: [ws nextPut: c. i := i + 1]].
        ^ws contents
    ]
    copyReplacingRegex: p with: rep [
        | ws last |
        ws := WriteStream on: String new.
        last := 1.
        (self allRegexMatchObjects: p) do: [:m |
            ws nextPutAll: (self copyFrom: last to: m start - 1).
            ws nextPutAll: (self expandRegexReplacement: rep with: m).
            last := m stop + 1].
        ws nextPutAll: (self copyFrom: last to: self size).
        ^ws contents
    ]
    splitOnRegex: p [
        | out last |
        out := OrderedCollection new.
        last := 1.
        (self allRegexMatchObjects: p) do: [:m |
            m stop >= m start ifTrue: [
                out add: (self copyFrom: last to: m start - 1).
                last := m stop + 1]].
        out add: (self copyFrom: last to: self size).
        ^out asArray
    ]
]
