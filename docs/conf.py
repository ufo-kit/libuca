# -*- coding: utf-8 -*-

import re

def get_version():
    patterns = [
            r'^set\(UCA_VERSION_MAJOR "(\d*)"\)',
            r'^set\(UCA_VERSION_MINOR "(\d*)"\)',
            r'^set\(UCA_VERSION_PATCH "(\d*)"\)'
            ]
    version = ["0", "0", "0"]

    with open('../CMakeLists.txt', 'r') as f:
        lines = f.readlines()
        major_pattern = r'^set\(UCA_VERSION_MAJOR "(\d*)"\)'

        for line in lines:
            for i, pattern in enumerate(patterns):
                m = re.match(pattern, line)

                if m:
                    version[i] = m.group(1)

    return '.'.join(version)

extensions = []
templates_path = ['_templates']
source_suffix = '.rst'
master_doc = 'index'

project = u'libuca'
copyright = u'2014, Matthias Vogelgesang'

version = get_version()
release = version

exclude_patterns = ['_build']
pygments_style = 'sphinx'

html_theme = 'default'

html_static_path = ['_static']
htmlhelp_basename = 'libucadoc'

latex_documents = [
  ('index', 'libuca.tex', u'libuca Documentation',
   u'Matthias Vogelgesang', 'manual'),
]

man_pages = [
    ('index', 'libuca', u'libuca Documentation',
     [u'Matthias Vogelgesang'], 1)
]
