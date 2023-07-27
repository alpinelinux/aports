from typing import Callable, Generic, Optional, Type, TypeVar

__all__ = ["Getable", "Getter", "Setter", "Deleter"]

OT = TypeVar("OT", contravariant=True)  # Owner Type
VT_co = TypeVar("VT_co", covariant=True)  # Covariant Value Type
VT_contra = TypeVar("VT_contra", contravariant=True)  # Contravariant Value Type

try:
    from typing import Protocol
except ImportError:
    Protocol = Generic


class Getable(Protocol[OT, VT_co]):
    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT_co:
        ...


Getter = Getable[OT, Callable[[], VT_co]]
Setter = Getable[OT, Callable[[VT_contra], None]]
Deleter = Getable[OT, Callable[[], None]]
