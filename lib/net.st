"net.st - TCP sockets, URLs and a small HTTP client (uses the CREXX rxsocket library)"

Object subclass: Socket [
    | handle buffer |
    Socket class >> primNew [ <primitive: sockNew> ^-1 ]
    Socket class >> primClose: h [ <primitive: sockClose> ^-1 ]
    Socket class >> primConnect: h host: host port: p [ <primitive: sockConnect> ^-1 ]
    Socket class >> primTls: h host: host port: p [ <primitive: sockConnectTls> ^-1 ]
    Socket class >> primBind: h host: host port: p [ <primitive: sockBind> ^-1 ]
    Socket class >> primListen: h backlog: n [ <primitive: sockListen> ^-1 ]
    Socket class >> primAccept: h [ <primitive: sockAccept> ^-1 ]
    Socket class >> primSend: h data: s [ <primitive: sockSend> ^-1 ]
    Socket class >> primRecv: h max: n [ <primitive: sockRecv> ^'' ]
    Socket class >> primPending: h [ <primitive: sockPending> ^0 ]
    Socket class >> primTimeout: h ms: n [ <primitive: sockTimeout> ^-1 ]
    Socket class >> primStatus: h [ <primitive: sockStatus> ^-1 ]
    Socket class >> primError: h [ <primitive: sockError> ^'' ]
    Socket class >> primPeer: h [ <primitive: sockPeer> ^'' ]
    Socket class >> primLocal: h [ <primitive: sockLocal> ^'' ]
    Socket class >> primNoDelay: h flag: n [ <primitive: sockNoDelay> ^-1 ]
    Socket class >> primShutdown: h how: n [ <primitive: sockShutdown> ^-1 ]

    Socket class >> new [ ^self basicNew setHandle: Socket primNew ]
    Socket class >> on: aHandle [ ^self basicNew setHandle: aHandle ]
    Socket class >> connectTo: host port: p [ ^self new connectTo: host port: p ]
    Socket class >> secureConnectTo: host port: p [ ^self new secureConnectTo: host port: p ]
    Socket class >> listenOn: p [ ^self new listenOn: p ]

    setHandle: h [ handle := h. buffer := ''. self timeout: 10000 ]
    handle [ ^handle ]
    timeout: ms [ ^Socket primTimeout: handle ms: ms ]
    noDelay: aBoolean [ ^Socket primNoDelay: handle flag: (aBoolean ifTrue: [1] ifFalse: [0]) ]
    status [ ^Socket primStatus: handle ]
    lastError [ ^Socket primError: handle ]
    isOpen [ ^handle notNil and: [handle > 0] ]
    peer [ ^Socket primPeer: handle ]
    localAddress [ ^Socket primLocal: handle ]
    pending [ ^Socket primPending: handle ]

    checkResult: rc what: what [
        rc = 0 ifFalse: [^self error: what, ' failed: ', self lastError].
        ^self
    ]
    connectTo: host port: p [
        ^self checkResult: (Socket primConnect: handle host: host port: p) what: 'connect to ', host, ':', p printString
    ]
    secureConnectTo: host port: p [
        ^self checkResult: (Socket primTls: handle host: host port: p) what: 'TLS connect to ', host, ':', p printString
    ]
    bindTo: host port: p [
        ^self checkResult: (Socket primBind: handle host: host port: p) what: 'bind ', host, ':', p printString
    ]
    listen: backlog [ ^self checkResult: (Socket primListen: handle backlog: backlog) what: 'listen' ]
    listenOn: p [ self bindTo: '127.0.0.1' port: p. ^self listen: 5 ]
    "wait for a connection; answers a new Socket"
    accept [
        | h |
        h := Socket primAccept: handle.
        h > 0 ifFalse: [^self error: 'accept failed: ', self lastError].
        ^Socket on: h
    ]

    send: aString [
        | n |
        n := Socket primSend: handle data: aString.
        n < 0 ifTrue: [^self error: 'send failed: ', self lastError].
        ^n
    ]
    nextPutAll: aString [ ^self send: aString ]
    sendLine: aString [ ^self send: aString, (String with: Character cr with: Character nl) ]
    shutdownWrite [ ^Socket primShutdown: handle how: 1 ]

    "read up to n bytes (what is buffered first); '' at end of stream or timeout"
    receive: n [
        | s |
        buffer isEmpty ifFalse: [
            s := buffer size <= n ifTrue: [buffer] ifFalse: [buffer copyFrom: 1 to: n].
            buffer := buffer copyFrom: s size + 1 to: buffer size.
            ^s].
        ^Socket primRecv: handle max: n
    ]
    fill [
        | s |
        s := Socket primRecv: handle max: 4096.
        s isEmpty ifTrue: [^false].
        buffer := buffer, s.
        ^true
    ]
    "answers the next line without its line terminator, nil at end of stream"
    receiveLine [
        | i line |
        [i := buffer indexOf: Character nl.
         i = 0] whileTrue: [self fill ifFalse: [
            buffer isEmpty ifTrue: [^nil].
            line := buffer. buffer := ''. ^line]].
        line := buffer copyFrom: 1 to: i - 1.
        buffer := buffer copyFrom: i + 1 to: buffer size.
        (line notEmpty and: [line last = Character cr]) ifTrue: [line := line copyFrom: 1 to: line size - 1].
        ^line
    ]
    receiveExactly: n [
        | s |
        [buffer size < n and: [self fill]] whileTrue.
        buffer size <= n ifTrue: [s := buffer. buffer := ''. ^s].
        s := buffer copyFrom: 1 to: n.
        buffer := buffer copyFrom: n + 1 to: buffer size.
        ^s
    ]
    receiveAll [
        | s |
        [self fill] whileTrue.
        s := buffer. buffer := ''.
        ^s
    ]
    close [
        self isOpen ifTrue: [Socket primClose: handle. handle := 0].
        ^self
    ]
    printOn: aStream [ aStream nextPutAll: 'a Socket(', handle printString, ')' ]
]

Object subclass: URL [
    | scheme host port path query |
    URL class >> fromString: s [ ^self new parse: s ]
    scheme [ ^scheme ]
    host [ ^host ]
    port [ ^port ]
    path [ ^path ]
    query [ ^query ]
    isSecure [ ^scheme = 'https' ]
    requestTarget [ ^query isEmpty ifTrue: [path] ifFalse: [path, '?', query] ]
    parse: s [
        | rest i hostport j |
        rest := s.
        i := rest indexOfSubCollection: '://'.
        i > 0 ifTrue: [scheme := (rest copyFrom: 1 to: i - 1) asLowercase. rest := rest copyFrom: i + 3 to: rest size]
              ifFalse: [scheme := 'http'].
        i := rest indexOf: $/.
        i = 0 ifTrue: [hostport := rest. rest := '/'] ifFalse: [hostport := rest copyFrom: 1 to: i - 1. rest := rest copyFrom: i to: rest size].
        j := hostport indexOf: $:.
        j = 0 ifTrue: [host := hostport. port := scheme = 'https' ifTrue: [443] ifFalse: [80]]
              ifFalse: [host := hostport copyFrom: 1 to: j - 1. port := (hostport copyFrom: j + 1 to: hostport size) asInteger].
        i := rest indexOf: $?.
        i = 0 ifTrue: [path := rest. query := '']
              ifFalse: [path := rest copyFrom: 1 to: i - 1. query := rest copyFrom: i + 1 to: rest size].
        ^self
    ]
    printOn: aStream [
        aStream nextPutAll: scheme, '://', host, ':', port printString, self requestTarget
    ]
]

Object subclass: HttpResponse [
    | status reason headers body |
    status [ ^status ]
    reason [ ^reason ]
    headers [ ^headers ]
    body [ ^body ]
    status: n reason: r headers: h body: b [ status := n. reason := r. headers := h. body := b ]
    header: name [ ^headers at: name asLowercase ifAbsent: [nil] ]
    isSuccess [ ^status >= 200 and: [status < 300] ]
    printOn: aStream [ aStream nextPutAll: 'HttpResponse(', status printString, ' ', reason, ')' ]
]

Object subclass: HttpClient [
    HttpClient class >> get: urlString [ ^self request: 'GET' url: urlString body: nil ]
    HttpClient class >> post: urlString body: aString [ ^self request: 'POST' url: urlString body: aString ]
    HttpClient class >> getString: urlString [ ^(self get: urlString) body ]

    HttpClient class >> request: method url: urlString body: aBody [
        | url sock req raw |
        url := URL fromString: urlString.
        sock := Socket new.
        sock timeout: 15000.
        url isSecure ifTrue: [sock secureConnectTo: url host port: url port]
                     ifFalse: [sock connectTo: url host port: url port].
        req := WriteStream on: String new.
        req nextPutAll: method, ' ', url requestTarget, ' HTTP/1.1', self crlf.
        req nextPutAll: 'Host: ', url host, self crlf.
        req nextPutAll: 'User-Agent: Smalltalk-CREXX', self crlf.
        req nextPutAll: 'Accept: */*', self crlf.
        req nextPutAll: 'Connection: close', self crlf.
        aBody notNil ifTrue: [
            req nextPutAll: 'Content-Length: ', aBody size printString, self crlf.
            req nextPutAll: 'Content-Type: application/x-www-form-urlencoded', self crlf].
        req nextPutAll: self crlf.
        aBody notNil ifTrue: [req nextPutAll: aBody].
        sock send: req contents.
        raw := sock receiveAll.
        sock close.
        ^self parseResponse: raw
    ]
    HttpClient class >> crlf [ ^String with: Character cr with: Character nl ]

    HttpClient class >> parseResponse: raw [
        | sep head lines statusLine parts headers body i name value te |
        sep := raw indexOfSubCollection: self crlf, self crlf.
        sep = 0 ifTrue: [^self error: 'malformed HTTP response'].
        head := raw copyFrom: 1 to: sep - 1.
        body := raw copyFrom: sep + 4 to: raw size.
        lines := OrderedCollection new.
        [head isEmpty] whileFalse: [
            i := head indexOf: Character nl.
            i = 0 ifTrue: [lines add: head. head := '']
                  ifFalse: [lines add: ((head copyFrom: 1 to: i - 1) copyWithout: Character cr).
                            head := head copyFrom: i + 1 to: head size]].
        statusLine := lines first.
        parts := statusLine subStrings: ' '.
        headers := Dictionary new.
        lines removeFirst.
        lines do: [:l |
            i := l indexOf: $:.
            i > 0 ifTrue: [
                name := (l copyFrom: 1 to: i - 1) asLowercase.
                value := (l copyFrom: i + 1 to: l size) trimSeparators.
                headers at: name put: value]].
        te := headers at: 'transfer-encoding' ifAbsent: [nil].
        (te notNil and: [te asLowercase includesSubstring: 'chunked']) ifTrue: [body := self unchunk: body].
        ^HttpResponse new
            status: (parts at: 2) asInteger
            reason: ((parts size > 2) ifTrue: [(parts copyFrom: 3 to: parts size) inject: '' into: [:a :w | a isEmpty ifTrue: [w] ifFalse: [a, ' ', w]]] ifFalse: [''])
            headers: headers body: body
    ]

    HttpClient class >> hexValue: aString [
        | n |
        n := 0.
        aString asLowercase do: [:c | n := n * 16 + ('0123456789abcdef' indexOf: c) - 1].
        ^n
    ]
    HttpClient class >> unchunk: data [
        | out pos i size hex |
        out := WriteStream on: String new.
        pos := 1.
        [pos <= data size] whileTrue: [
            i := data indexOfSubCollection: self crlf startingAt: pos.
            i = 0 ifTrue: [^out contents].
            hex := (data copyFrom: pos to: i - 1) trimSeparators.
            (hex indexOf: $;) > 0 ifTrue: [hex := hex copyFrom: 1 to: (hex indexOf: $;) - 1].
            size := self hexValue: hex.
            size = 0 ifTrue: [^out contents].
            out nextPutAll: (data copyFrom: i + 2 to: i + 1 + size).
            pos := i + 2 + size + 2].
        ^out contents
    ]
]
