from setuptools import setup

setup(
    name='tangouca',    # jeez
    version='0.0.1',
    author='Matthias Vogelgesang',
    author_email='matthias.vogelgesang@kit.edu',
    url='http://ufo.kit.edu',
    license='(?)',
    description='TANGO server for libuca',
    long_description='TANGO server for libuca',
    scripts=['Uca'],
    install_requires=['PyTango']
)
