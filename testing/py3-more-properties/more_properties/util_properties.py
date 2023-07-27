from dataclasses import dataclass
from typing import Generic, Optional, Type, TypeVar

from more_properties.property import Property
from more_properties.types import Getable

__all__ = [
    "NamedProperty",
    "WrappedProperty",
]

OT = TypeVar("OT", contravariant=True)  # Owner Type
VT = TypeVar("VT")  # Value Type


@dataclass
class NamedProperty(Property[OT, VT]):
    name: Optional[str] = None

    def __set_name__(self, owner: Type[OT], name: str) -> None:
        self.name = name


@dataclass
class TrivialWrapper(Generic[OT, VT]):
    getable: Getable[OT, VT]

    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT:
        getable: Getable[OT, VT] = self.__dict__["getable"]
        return getable.__get__(instance, owner)


class WrappedProperty(Property[OT, VT]):
    wrapper: type = TrivialWrapper

    def __post_init__(self) -> None:
        fget = self.__dict__["fget"]

        if fget is not None and not isinstance(fget, self.wrapper):
            setattr(self, "fget", self.wrapper(fget))

        fset = self.__dict__["fset"]

        if fset is not None and not isinstance(fset, self.wrapper):
            setattr(self, "fset", self.wrapper(fset))

        fdel = self.__dict__["fdel"]

        if fdel is not None and not isinstance(fdel, self.wrapper):
            setattr(self, "fdel", self.wrapper(fdel))

        super().__post_init__()
