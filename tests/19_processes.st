"cooperative processes, semaphores, shared queues"
| p s q mutex count log results done |
Transcript showCr: 'start'.
Eval [
    p := [Transcript showCr: 'in forked process'] fork.
    Transcript showCr: 'after fork'.
    p printNl
]
p isTerminated printNl.

"wait for a process with a semaphore"
s := Semaphore new.
[Transcript showCr: 'worker running'. s signal] fork.
s wait.
Transcript showCr: 'main resumed'.

"producer and consumer"
q := SharedQueue new.
results := OrderedCollection new.
[1 to: 5 do: [:i | q nextPut: i * i]. q nextPut: nil] fork.
[| v | [(v := q next) notNil] whileTrue: [results add: v]] fork.
Processor yield.
results printNl.

"priorities: a higher priority process runs at once, a lower one when the statement ends"
log := OrderedCollection new.
Eval [
    [log add: 'low'] forkAt: Processor userBackgroundPriority.
    [log add: 'same'] fork.
    [log add: 'high'] forkAt: Processor userInterruptPriority.
    log add: 'main'.
    Processor yield.
    log add: 'after yield'
]
log printNl.

"mutual exclusion"
mutex := Semaphore forMutualExclusion.
count := 0.
done := Semaphore new.
1 to: 3 do: [:i | [mutex critical: [count := count + i]. done signal] fork].
3 timesRepeat: [done wait].
count printNl.

"termination"
p := [Transcript showCr: 'never'] newProcess.
p isSuspended printNl.
p terminate.
p isTerminated printNl.
p := [[Transcript showCr: 'running'. Processor activeProcess terminate. Transcript showCr: 'not reached'] ensure: [Transcript showCr: 'cleanup']] fork.
Processor yield.

"errors in a process do not stop the others"
[1/0] fork.
[Transcript showCr: 'still alive'] fork.
Processor yield.

"resume and suspend"
p := [Transcript showCr: 'resumed process ran'] newProcess.
p resume.
Processor yield.
Processor activeProcess printNl.
Processor activePriority printNl.
(Delay forMilliseconds: 50) wait.
Transcript showCr: 'after delay'.
(Semaphore new) wait.
Transcript showCr: 'end'.
