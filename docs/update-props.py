#!/usr/bin/env

import sys
from gi.repository import Uca, GLib, GObject


def get_default_optional(prop):
    if hasattr(prop, 'default_value'):
        return '    | *Default:* {}'.format(prop.default_value)
    return ''


def get_range_optional(prop):
    if hasattr(prop, 'minimum') and hasattr(prop, 'maximum'):
        return '    | *Range:* [{}, {}]'.format(prop.minimum, prop.maximum)
    return ''


def get_type_description(value_type):
    mapping = {
        GObject.TYPE_BOOLEAN: 'bool',
        GObject.TYPE_CHAR: 'char',
        GObject.TYPE_UCHAR: 'unsigned char',
        GObject.TYPE_FLOAT: 'float',
        GObject.TYPE_INT: 'int',
        GObject.TYPE_UINT: 'unsigned int',
        GObject.TYPE_LONG: 'long',
        GObject.TYPE_DOUBLE: 'double',
        GObject.TYPE_STRING: 'string',
        GObject.TYPE_ENUM: 'enum',
    }

    return mapping.get(value_type, None)


def get_prop_string(prop):
    template = """
{type} **{name}**
    {blurb}

{optionals}"""

    optionals = '\n'.join(opt for opt in (get_default_optional(prop), get_range_optional(prop)) if opt)

    return template.format(name=prop.name, blurb=prop.blurb,
                           type=get_type_description(prop.value_type),
                           optionals=optionals)

def output(camera, name):
    template = """
{name}
{name_underline}
{props}"""

    name_underline = '='*len(name)

    stream = open('{}.rst'.format(name), 'w')
    props = '\n'.join((get_prop_string(prop) for prop in camera.props))
    stream.write(template.format(name=name, name_underline=name_underline,
                                 props=props))


if __name__ == '__main__':
    pm = Uca.PluginManager()

    if len(sys.argv) < 2:
        print("Usage: update-props.py NAME")
        sys.exit(0)

    name = sys.argv[1]

    try:
        output(pm.get_camerav(name, []), name)
    except GLib.GError as e:
        print("Could not query {}: {}".format(name, e.message))
