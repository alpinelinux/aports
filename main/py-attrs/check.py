import attr

@attr.s
class SomeClass(object):
    a_number = attr.ib(default=42)
    list_of_numbers = attr.ib(default=attr.Factory(list))

a = SomeClass()
assert a.a_number == 42
assert isinstance(a.list_of_numbers, list)
