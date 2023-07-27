from dataclasses import dataclass
from typing import Optional, Type, TypeVar, Union

from more_properties.class_property import ClassProperty, StaticProperty
from more_properties.types import Deleter
from more_properties.util_properties import NamedProperty

__all__ = [
    "cached_property",
    "cached_class_property",
    "cached_static_property",
]

OT = TypeVar("OT", contravariant=True)  # Owner Type
VT = TypeVar("VT")  # Value Type


class Uncached:
    pass


Cache = Union[VT, Uncached]


@dataclass
class CachedProperty(NamedProperty[OT, VT]):
    @property
    def cache_name(self) -> str:
        if self.name is None:
            raise AttributeError(f"Property {self!r} not assigned to class")

        return f"__{self.name}_cache"

    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT:
        cache_name = self.cache_name

        if cache_name not in instance.__dict__:
            instance.__dict__[cache_name] = super().__get__(instance, owner)

        value: VT = instance.__dict__[cache_name]

        return value

    def __set__(self, instance: OT, value: VT) -> None:
        # Mypy seems to unwrap the descriptor recursively, while Python only does it once
        clear_cache: Deleter[OT] = self.clear_cache  # type: ignore

        clear_cache.__get__(instance, type(instance))()

        super().__set__(instance, value)

    def __delete__(self, instance: OT) -> None:
        # Mypy seems to unwrap the descriptor recursively, while Python only does it once
        clear_cache: Deleter[OT] = self.clear_cache  # type: ignore

        clear_cache.__get__(instance, type(instance))()

        super().__delete__(instance)

    @property
    def clear_cache(self) -> Deleter[OT]:
        def clear_cache(instance: OT) -> None:
            cache_name = self.cache_name

            if cache_name in instance.__dict__:
                delattr(instance, cache_name)

        # Mypy doesn't recognize functions as Getable
        return clear_cache  # type: ignore


@dataclass
class CachedClassProperty(CachedProperty[OT, VT], ClassProperty[OT, VT]):
    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT:
        cache_name = self.cache_name

        if cache_name in owner.__dict__:
            value: VT = owner.__dict__[cache_name]

            return value

        value = super(CachedProperty, self).__get__(instance, owner)

        setattr(owner, cache_name, value)

        return value

    @property
    def clear_cache(self) -> Deleter[OT]:
        def clear_cache(owner: Type[OT]) -> None:
            cache_name = self.cache_name

            if cache_name in owner.__dict__:
                delattr(owner, cache_name)

        return classmethod(clear_cache)


@dataclass
class CachedStaticProperty(CachedProperty[OT, VT], StaticProperty[OT, VT]):
    value: Cache[VT] = Uncached()

    def __get__(self, instance: Optional[OT], owner: Type[OT]) -> VT:
        if isinstance(self.value, Uncached):
            self.value = super(CachedProperty, self).__get__(instance, owner)

        return self.value

    @property
    def clear_cache(self) -> Deleter[OT]:
        def clear_cache() -> None:
            self.value = Uncached()

        return staticmethod(clear_cache)


cached_property = CachedProperty
cached_class_property = CachedClassProperty
cached_static_property = CachedStaticProperty
