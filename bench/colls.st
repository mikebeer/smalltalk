| t oc d s a |
t := Time millisecondClock.
oc := OrderedCollection new.
1 to: 50000 do: [:i | oc add: i * 2].
(oc inject: 0 into: [:x :y | x + y]) printNl.
d := Dictionary new.
1 to: 20000 do: [:i | d at: i put: i * i].
(d at: 777) printNl.
a := (1 to: 20000) asArray collect: [:i | (i * 7919) \\ 10007].
(a asSortedCollection: [:x :y | x <= y]) first printNl.
s := Set new. a do: [:e | s add: e]. s size printNl.
((1 to: 30000) select: [:i | i even]) size printNl.
(Time millisecondClock - t) displayNl.
