from dataclasses import dataclass, replace
from typing import Generic, Optional, Type, TypeVar

from more_properties.types import Deleter, Getter, Setter

__all__ = [
    "Property",
    "property",
]

OT = TypeVar("OT", contravariant=True)  # Owner Type
VT = TypeVar("VT")  # Value Type


@dataclass
class Property(Generic[OT, VT]):
    fget: Optional[Getter[OT, VT]] = None
    fset: Optional[Setter[OT, VT]] = None
    fdel: Optional[Deleter[OT]] = None
    doc: Optional[str] = None

    def __post_init__(self) -> None:
        self.__doc__ = (
            self.doc
            if self.doc is not None
            else getattr(self.__dict__.get("fget"), "__doc__", None)
        )

    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT:
        fget: Optional[Getter[OT, VT]] = self.__dict__["fget"]

        if fget is None:
            raise AttributeError("unreadable attribute")

        return fget.__get__(instance, owner)()

    def __set__(self, instance: OT, value: VT) -> None:
        fset = self.__dict__["fset"]

        if fset is None:
            raise AttributeError("can't set attribute")

        fset.__get__(instance, type(instance))(value)

    def __delete__(self, instance: OT) -> None:
        fdel = self.__dict__["fdel"]

        if fdel is None:
            raise AttributeError("can't delete attribute")

        fdel.__get__(instance, type(instance))()

    def getter(self, func: Getter[OT, VT]) -> "Property[OT, VT]":
        return replace(self, fget=func)

    def setter(self, func: Setter[OT, VT]) -> "Property[OT, VT]":
        return replace(self, fset=func)

    def deleter(self, func: Deleter[OT]) -> "Property[OT, VT]":
        return replace(self, fdel=func)


property = Property
