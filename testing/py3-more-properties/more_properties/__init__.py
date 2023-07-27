from more_properties.cached_property import (
    cached_class_property,
    cached_property,
    cached_static_property,
)
from more_properties.class_property import class_property, static_property
from more_properties.property import property

__all__ = [
    "property",
    "class_property",
    "classproperty",
    "static_property",
    "staticproperty",
    "cached_property",
    "cached_class_property",
    "cached_static_property",
]

__version__ = "1.1.1"

# Providing aliases for consistency with classmethod and staticmethod
classproperty = class_property
staticproperty = static_property
