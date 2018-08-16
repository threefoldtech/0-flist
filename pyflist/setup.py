from distutils.core import setup, Extension

flist = Extension(
    'pyflist',
    include_dirs=['../libflist/'],
    libraries=['snappy', 'z', 'm', 'b2', 'sqlite3', 'tar', 'capnp_c', 'hiredis'],
    sources=['pyflist.c'],
    extra_compile_args=['-std=c99', '-fopenmp'],
    extra_link_args=['-fopenmp', '../libflist/libflist.a'],
)

setup(
    name='pyflist',
    version = '1.0',
    description='Flist Python Library',
    long_description='Flist Python Library',
    url='https://github.com/threefoldtech/0-flist',
    author='Maxime Daniel',
    author_email='maxime@gig.tech',
    license='Apache 2.0',
    ext_modules=[flist]
)
