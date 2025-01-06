try:
    from ._version import __version__ as everestpy_version
except ImportError:
    everestpy_version = '0.0.0'

try:
    from .everestpy import *
except ImportError:
    try:
        from everestpy import *
    except ImportError:
        # Unable to import everestpy which can happen during setup.py
        pass
