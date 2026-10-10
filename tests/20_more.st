"Duration, ByteArray, dependents, Rectangle, CompiledMethod, allInstances"
| d1 d2 dt |
d1 := 90061 seconds. d1 printNl. d1 days printNl. d1 hours printNl. d1 minutes printNl. d1 seconds printNl.
(d1 + 5 minutes) printNl. (d1 * 2) printNl. (2 hours / 30 minutes) printNl. (5 seconds < 1 minutes) printNl.
dt := DateTime year: 2026 month: 10 day: 10 hour: 8 minute: 30 second: 0.
(dt + 36 hours) printNl.
((dt + 36 hours) - dt) printNl.
(dt - 1 days) printNl.
((Date year: 2026 month: 12 day: 25) - (Date year: 2026 month: 10 day: 10)) printNl.
#[1 2 255] printNl. #[1 2 255] class printNl. #[1 2 255] hex printNl.
(ByteArray new: 3) printNl.
'AB' asByteArray printNl. #[72 105] asString printNl.
Object subclass: Model [ |v| v: x [ v := x. self changed: #v ] ]
Object subclass: Watcher [ update: a [ Transcript showCr: 'changed ', a printString ] ]
| m | m := Model new. m addDependent: Watcher new. m v: 3. m dependents size printNl.
m release. m v: 4. m dependents size printNl.
((0@0) corner: (4@3)) printNl. ((0@0) corner: (4@3)) area printNl.
(((0@0) corner: (4@4)) intersect: ((2@2) corner: (6@6))) printNl.
(((0@0) corner: (4@4)) containsPoint: 2@2) printNl.
(Model >> #v:) printNl. (Model lookupSelector: #printNl) printNl.
Model new. Model allInstances size printNl.
