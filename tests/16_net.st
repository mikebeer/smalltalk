"sockets over loopback, URL parsing and HTTP response parsing"
| srv cli acc crlf r u |
srv := Socket listenOn: 19877.
cli := Socket connectTo: '127.0.0.1' port: 19877.
acc := srv accept.
cli sendLine: 'hello server'.
Transcript showCr: 'server got: ', acc receiveLine.
acc send: 'line one'; sendLine: ''; sendLine: 'line two'.
Transcript showCr: 'client got: ', cli receiveLine.
Transcript showCr: 'client got: ', cli receiveLine.
cli send: 'abcdefghij'.
Transcript showCr: 'exactly 4: ', (acc receiveExactly: 4).
Transcript showCr: 'rest: ', (acc receive: 100).
cli close.
acc receiveLine printNl.
acc close. srv close.
cli isOpen printNl.
[Socket connectTo: '127.0.0.1' port: 1] on: Error do: [:e | Transcript showCr: 'connect failed (expected)'].

u := URL fromString: 'https://example.com/a/b?x=1&y=2'.
u scheme printNl. u host printNl. u port printNl. u path printNl. u query printNl. u isSecure printNl.
u := URL fromString: 'http://localhost:8080'.
u port printNl. u path printNl. u requestTarget printNl.

crlf := HttpClient crlf.
r := HttpClient parseResponse: 'HTTP/1.1 404 Not Found', crlf, 'Content-Type: text/plain', crlf, 'X-Test: yes', crlf, crlf, 'missing'.
r status printNl. r reason printNl. r body printNl. (r header: 'x-test') printNl. (r header: 'nothing') printNl. r isSuccess printNl.
r := HttpClient parseResponse: 'HTTP/1.1 200 OK', crlf, 'Transfer-Encoding: chunked', crlf, crlf, '5', crlf, 'hello', crlf, 'A', crlf, ' world!!!!', crlf, '0', crlf, crlf.
r body printNl. r isSuccess printNl.
