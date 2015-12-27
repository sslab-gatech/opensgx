#!/usr/bin/env python2
#
# Copyright 2011 Nick Mathewson, Michael Stone
#
#  You may do anything with this work that copyright law would normally
#  restrict, so long as you retain the above notice(s) and this license
#  in all redistributed copies and derived works.  There is no warranty.

"""
  This module contins a general-purpose specialization-based
  templating mechanism.  Chutney uses it for string-substitution to
  build torrc files and other such things.

  First, there are a few classes to implement "Environments".  An
  "Environment" is like a dictionary, but delegates keys that it can't
  find to a "parent" object, which can be an environment or a dict.

>>> base = Environ(foo=99, bar=600)
>>> derived1 = Environ(parent=base, bar=700, quux=32)
>>> base["foo"]
99
>>> sorted(base.keys())
['bar', 'foo']
>>> derived1["foo"]
99
>>> base["bar"]
600
>>> derived1["bar"]
700
>>> derived1["quux"]
32
>>> sorted(derived1.keys())
['bar', 'foo', 'quux']

    You can declare an environment subclass with methods named
    _get_foo() to implement a dictionary whose 'foo' item is
    calculated on the fly.

>>> class Specialized(Environ):
...    def __init__(self, p=None, **kw):
...       Environ.__init__(self, p, **kw)
...    def _get_expensive_value(self, my):
...       return "Let's pretend this is hard to compute"
...
>>> s = Specialized(base, quux="hi")
>>> s["quux"]
'hi'
>>> s['expensive_value']
"Let's pretend this is hard to compute"
>>> sorted(s.keys())
['bar', 'expensive_value', 'foo', 'quux']

   There's an internal class that extends Python's string.Template
   with a slightly more useful syntax for us.  (It allows more characters
   in its key types.)

>>> bt = _BetterTemplate("Testing ${hello}, $goodbye$$, $foo , ${a:b:c}")
>>> bt.safe_substitute({'a:b:c': "4"}, hello=1, goodbye=2, foo=3)
'Testing 1, 2$, 3 , 4'

   Finally, there's a Template class that implements templates for
   simple string substitution.  Variables with names like $abc or
   ${abc} are replaced by their values; $$ is replaced by $, and
   ${include:foo} is replaced by the contents of the file "foo".

>>> t = Template("${include:/dev/null} $hi_there")
>>> sorted(t.freevars())
['hi_there']
>>> t.format(dict(hi_there=99))
' 99'
>>> t2 = Template("X$${include:$fname} $bar $baz")
>>> t2.format(dict(fname="/dev/null", bar=33, baz="$foo", foo=1337))
'X 33 1337'
>>> sorted(t2.freevars({'fname':"/dev/null"}))
['bar', 'baz', 'fname']

"""

from __future__ import print_function
from __future__ import with_statement

import string
import os

# class _KeyError(KeyError):
#    pass

_KeyError = KeyError


class _DictWrapper(object):

    """Base class to implement a dictionary-like object with delegation.
       To use it, implement the _getitem method, and pass the optional
       'parent' argument to the constructor.

       Lookups are satisfied first by calling _getitem().  If that
       fails with KeyError but the parent is present, lookups delegate
       to _parent.
    """
    # Fields
    # _parent: the parent object that lookups delegate to.

    def __init__(self, parent=None):
        self._parent = parent

    def _getitem(self, key, my):
        raise NotImplemented()

    def __getitem__(self, key):
        return self.lookup(key, self)

    def lookup(self, key, my):
        """As self[key], but parents are told when doing their lookups that
           the lookup is relative to a specialized environment 'my'.  This
           is helpful when a parent environment has a value that depends
           on other values.
        """
        try:
            return self._getitem(key, my)
        except KeyError:
            pass
        if self._parent is None:
            raise _KeyError(key)

        try:
            lookup = self._parent.lookup
        except AttributeError:
            try:
                return self._parent[key]
            except KeyError:
                raise _KeyError(key)

        try:
            return lookup(key, my)
        except KeyError:
            raise _KeyError(key)


class Environ(_DictWrapper):

    """An 'Environ' is used to define a set of key-value mappings with a
       fall-back parent Environ.  When looking for keys in the
       Environ, any keys not found in this Environ are searched for in
       the parent.

       >>> animal = Environ(mobile=True,legs=4,can_program=False,can_meow=False)
       >>> biped = Environ(animal,legs=2)
       >>> cat = Environ(animal,can_meow=True)
       >>> human = Environ(biped,feathers=False,can_program=True)
       >>> chicken = Environ(biped,feathers=True)
       >>> human['legs']
       2
       >>> human['can_meow']
       False
       >>> human['can_program']
       True
       >>> cat['legs']
       4
       >>> cat['can_meow']
       True

       You can extend Environ to support values calculated at run-time by
       defining methods with names in the format _get_KEY():

       >>> class HomeEnv(Environ):
       ...    def __init__(self, p=None, **kw):
       ...       Environ.__init__(self, p, **kw)
       ...    def _get_dotemacs(self, my):
       ...       return os.path.join(my['homedir'], ".emacs")
       >>> h = HomeEnv(homedir="/tmp")
       >>> h['dotemacs']
       '/tmp/.emacs'

       The 'my' argument passed to these functions is the top-level
       dictionary that we're using for our lookup.  This is useful
       when defining values that depend on other values which might in
       turn be overridden:

       >>> class Animal(Environ):
       ...    def __init__(self, p=None, **kw):
       ...       Environ.__init__(self, p, **kw)
       ...    def _get_limbs(self, my):
       ...       return my['legs'] + my['arms']
       >>> a = Animal(legs=2,arms=2)
       >>> spider = Environ(a, legs=8,arms=0)
       >>> squid = Environ(a, legs=0,arms=10)
       >>> squid['limbs']
       10
       >>> spider['limbs']
       8

       Note that if _get_limbs() had been defined as
          'return self['legs']+self['arms']',
       both spider['limbs'] and squid['limbs'] would be given
       (incorrectly) as 4.
    """
    # Fields
    # _dict: dictionary holding the contents of this Environ that are
    #   not inherited from the parent and are not computed on the fly.

    def __init__(self, parent=None, **kw):
        _DictWrapper.__init__(self, parent)
        self._dict = kw

    def _getitem(self, key, my):
        try:
            return self._dict[key]
        except KeyError:
            pass

        fn = getattr(self, "_get_%s" % key, None)
        if fn is not None:
            try:
                rv = fn(my)
                return rv
            except _KeyError:
                raise KeyError(key)
        raise KeyError(key)

    def __setitem__(self, key, val):
        self._dict[key] = val

    def keys(self):
        s = set()
        s.update(self._dict.keys())
        if self._parent is not None:
            s.update(self._parent.keys())
        s.update(name[5:] for name in dir(self) if name.startswith("_get_"))
        return s


class IncluderDict(_DictWrapper):

    """Helper to implement ${include:} template substitution.  Acts as a
       dictionary that maps include:foo to the contents of foo (relative to
       a search path if foo is a relative path), and delegates everything else
       to a parent.
    """
    # Fields
    # _includePath: a list of directories to consider when searching
    #   for files given as relative paths.
    # _st_mtime: the most recent time when any of the files included
    #   so far has been updated.  (As seconds since the epoch).

    def __init__(self, parent, includePath=(".",)):
        """Create a new IncluderDict.  Non-include entries are delegated to
           parent.  Non-absolute paths are searched for relative to the
           paths listed in includePath.
        """
        _DictWrapper.__init__(self, parent)
        self._includePath = includePath
        self._st_mtime = 0

    def _getitem(self, key, my):
        if not key.startswith("include:"):
            raise KeyError(key)

        filename = key[len("include:"):]
        if os.path.isabs(filename):
            with open(filename, 'r') as f:
                stat = os.fstat(f.fileno())
                if stat.st_mtime > self._st_mtime:
                    self._st_mtime = stat.st_mtime
                return f.read()

        for elt in self._includePath:
            fullname = os.path.join(elt, filename)
            if os.path.exists(fullname):
                with open(fullname, 'r') as f:
                    stat = os.fstat(f.fileno())
                    if stat.st_mtime > self._st_mtime:
                        self._st_mtime = stat.st_mtime
                    return f.read()

        raise KeyError(key)

    def getUpdateTime(self):
        return self._st_mtime


class _BetterTemplate(string.Template):

    """Subclass of the standard string.Template that allows a wider range of
       characters in variable names.
    """

    idpattern = r'[a-z0-9:_/\.\-\/]+'

    def __init__(self, template):
        string.Template.__init__(self, template)


class _FindVarsHelper(object):

    """Helper dictionary for finding the free variables in a template.
       It answers all requests for key lookups affirmatively, and remembers
       what it was asked for.
    """
    # Fields
    # _dflts: a dictionary of default values to treat specially
    # _vars: a set of all the keys that we've been asked to look up so far.

    def __init__(self, dflts):
        self._dflts = dflts
        self._vars = set()

    def __getitem__(self, var):
        return self.lookup(var, self)

    def lookup(self, var, my):
        self._vars.add(var)
        try:
            return self._dflts[var]
        except KeyError:
            return ""


class Template(object):

    """A Template is a string pattern that allows repeated variable
       substitutions.  These syntaxes are supported:
          $var -- expands to the value of var
          ${var} -- expands to the value of var
          $$ -- expands to a single $
          ${include:filename} -- expands to the contents of filename

       Substitutions are performed iteratively until no more are possible.
    """

    # Okay, actually, we stop after this many substitutions to avoid
    # infinite loops
    MAX_ITERATIONS = 32

    # Fields
    # _pat: The base pattern string to start our substitutions from
    # _includePath: a list of directories to search when including a file
    #    by relative path.

    def __init__(self, pattern, includePath=(".",)):
        self._pat = pattern
        self._includePath = includePath

    def freevars(self, defaults=None):
        """Return a set containing all the free variables in this template"""
        if defaults is None:
            defaults = {}
        d = _FindVarsHelper(defaults)
        self.format(d)
        return d._vars

    def format(self, values):
        """Return a string containing this template, filled in with the
           values in the mapping 'values'.
        """
        values = IncluderDict(values, self._includePath)
        orig_val = self._pat
        nIterations = 0
        while True:
            v = _BetterTemplate(orig_val).substitute(values)
            if v == orig_val:
                return v
            orig_val = v
            nIterations += 1
            if nIterations > self.MAX_ITERATIONS:
                raise ValueError("Too many iterations in expanding template!")

if __name__ == '__main__':
    import sys
    if len(sys.argv) == 1:
        import doctest
        doctest.testmod()
        print("done")
    else:
        for fn in sys.argv[1:]:
            with open(fn, 'r') as f:
                t = Template(f.read())
                print(fn, t.freevars())
