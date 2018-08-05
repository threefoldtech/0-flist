from distutils.core import setup, Extension

g8storclient = Extension(
    'g8storclient',
    include_dirs=['../src/'],
    libraries=['snappy', 'z', 'm', 'b2'],
    extra_objects=['../src/lib0stor.a'],
    sources=['lib0stor-python.c'],
    extra_compile_args=['-std=c99'],
)

setup(
    name='g8storclient',
    version = '1.0',
    description='0-stor Legacy Library',
    long_description='0-stor Legacy Library',
    url='https://github.com/maxux/lib0stor',
    author='Maxime Daniel',
    author_email='maxime@gig.tech',
    license='Apache 2.0',
    ext_modules=[g8storclient]
)
