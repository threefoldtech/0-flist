# flistpy

cython based bindings for libflist

## Files

### cflist.pxd
contains all header informations/types from `stdint`, `time`, `libflist`

### flist.pyx
cython code that has access to cflist exposed functions/types.

### setup.py

```python3
from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize

setup(
    ext_modules = cythonize([Extension("flist", ["flist.pyx"], libraries=["flist"])], compiler_directives={'language_level' : "3"})
)
```

in there we specify extensions and compiler directives 
- `flist` extension name
- `flist.pyx` cython code
- `libraries` which libraries to link against in our case `libflist` (remove lib prefix)
- `compiler_diretives` set language_level to 3



## Example 

```

    cdef open(self):
        os.makedirs(self.mntpath, exist_ok=True)
        if os.path.exists(self.sqlite_db_path):
            raise MountPointHasDatabaseAlready("mount point already contains db")

        done = cflist.libflist_archive_extract(self.bpath, self.bmntpath)
        if not done:
            raise RuntimeError("extract/archive")
```

## Notes

- not direct assignment from python strings to `char*` (encode into bytes in another variable and set it to your pointer)
- if you choose to change LD_LIBRARY_PATH it's not guaranteed to be found in cython context so if any problem try putting it in `/usr/lib`

