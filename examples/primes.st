"Sieve of Eratosthenes and friends"
| sieve primes n |
n := 100.
sieve := Array new: n withAll: true.
sieve at: 1 put: false.
2 to: n sqrt floor do: [:i |
    (sieve at: i) ifTrue: [
        i * i to: n by: i do: [:j | sieve at: j put: false]]].
primes := (1 to: n) select: [:i | sieve at: i].
primes printNl.
primes size printNl.
(primes inject: 0 into: [:a :b | a + b]) printNl.
((1 to: 50) select: [:i | i isPrime]) printNl.
