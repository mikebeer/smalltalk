"Classes, inheritance, polymorphism and super"
Object subclass: Shape [
    | name |
    name [ ^name ]
    name: aString [ name := aString ]
    area [ ^self subclassResponsibility ]
    describe [ ^name, ' with area ', self area printString ]
    printOn: aStream [ aStream nextPutAll: 'Shape(', name, ')' ]
]
Shape subclass: Rectangle [
    | w h |
    Rectangle class >> width: aW height: aH [ ^self new setW: aW h: aH ]
    setW: aW h: aH [ w := aW. h := aH. name := 'Rectangle' ]
    area [ ^w * h ]
]
Rectangle subclass: Square [
    Square class >> side: s [ ^(self width: s height: s) setName ]
    setName [ name := 'Square' ]
]
Shape subclass: Circle [
    | r |
    Circle class >> radius: x [ ^self new setR: x ]
    setR: x [ r := x. name := 'Circle' ]
    area [ ^r * r * 355 / 113 ]
]
| shapes |
shapes := OrderedCollection new.
shapes add: (Rectangle width: 3 height: 4);
       add: (Square side: 5);
       add: (Circle radius: 2).
shapes do: [:s | s describe displayNl].
shapes printNl.
(shapes detect: [:s | s area > 20]) printNl.
(shapes inject: 0 into: [:sum :s | sum + s area]) printNl.
(shapes asSortedCollection: [:a :b | a area >= b area]) first printNl.
Shape new area.
