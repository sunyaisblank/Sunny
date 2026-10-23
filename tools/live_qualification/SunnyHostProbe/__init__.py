"""Separate, opt-in diagnostic surface. Does not modify the Sunny bridge bundle."""

from .probe import HostProbe


def create_instance(c_instance):
    """Create the separately selected qualification Control Surface."""
    return HostProbe(c_instance)
