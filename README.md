[![Build Status][opencflite-github-action-svg]][opencflite-github-action]

[opencflite-github]: https://github.com/gerickson/opencflite
[opencflite-github-action]: https://github.com/gerickson/opencflite/actions?query=workflow%3ABuild+branch%3Amain+event%3Apush
[opencflite-github-action-svg]: https://github.com/gerickson/opencflite/actions/workflows/build.yml/badge.svg?branch=main&event=push

Open CF-lite
============

# Introduction

This is the public, open source distribution of [Apple, Inc.'s
CoreFoundation framework](https://opensource.apple.com/source/CF/),
sometimes known as "CF-lite" because it does not contain every
facility available from the CoreFoundation framework found in
iOS/iPadOS/macOS/tvOS/watchOS. This distribution is refered to as Open
CF-lite to distinguish it from the official Apple release, and to
reflect the open source, community-based and -driven nature of this
project.

This release of Open CF-lite corresponds to the CoreFoundation
framework found in Mac OS X 10.7.4,
[CF-635.21](https://opensource.apple.com/source/CF/CF-635.21/).

This distribution differs from the official Apple, Inc. release of
CF-lite in that it is known to build and run on Mac OS X, Windows, and
Linux. It should probably be trivial to port to most other POSIX-based
environments (volunteers welcome!)

The goal of this port is to provide a feature-compatible, cross-
platform version of the official CoreFoundation framework. In general,
we do not propose extending functionality beyond the official Apple
release so that this project can serve as a drop-in replacement.

To repeat Apple's statement:

> What Apple is NOT interested in, with CF-lite:
* Everybody's little convenience methods. Just because "everybody has to
write their own", it does not follow immediately that it is a good
idea to add it to the system libraries. It is not a goal of CF to be a
"Swiss Army Knife"; that just increases the size of the binaries, and
the documentation and maintenance burden. Functions to reverse a
string or merge two arrays by taking alternate elements from the two
arrays are not compelling.

The current release of Open CF-lite is known to work on Windows at a
sufficient level to run the WebKit infrastructure.

# Getting Started with Open CF-lite

## Building Open CF-lite

If you are not using a prebuilt distribution of Open CF-lite,
building Open CF-lite should be a straightforward. Start with:

    % ./configure
    % make

The second `configure` step generates `Makefile` files from
`Makefile.in` files and only needs to be done once unless those input
files have changed.

Although not strictly necessary, the additional step of sanity
checking the build results is recommended:

    % make check

### Dependencies

In addition to depending on the C Standard Libraries, Open
CF-lite depends on:

  * [International Components for Unicode](http://icu-project.org/)
  * [Kqueue](https://github.com/mheily/libkqueue) (FreeBSD and Linux only)
  * [Time Zone Database](https://www.iana.org/time-zones)
  * [Universally Unique ID Library](http://e2fsprogs.sourceforge.net)
  * [zlib](http://zlib.net/)

The dependencies can either be satisfied by building them directly
from source, or on system such as Linux, installing them using a
package management system. For example, on Debian systems:

    % sudo apt-get install uuid-dev libicu-dev libkqueue-dev zlib1g-dev

#### A Comment about libkqueue

There are a number of issues in libkqueue prior to version 2.5.2 that
preclude it from working correctly to serve the needs of run loop
timers. Consequently, if your distribution does not have libkqueue
equal to or later than 2.5.2, you may need to build a suitable version
of libkqueue from source and install it.

If you want to modify or otherwise maintain the Open CF-lite build
system, see "Maintaining Open CF-lite" below for more information.

## Installing Open CF-lite

To install Open CF-lite for your use simply invoke:

    % make install

to install Open CF-lite in the location indicated by the --prefix
`configure` option (default "/usr/local"). If you intended an
arbitrarily relocatable Open CF-lite installation and passed
`--prefix=/` to `configure`, then you might use DESTDIR to, for
example install Open CF-lite in your user directory:

    % make DESTIDIR="${HOME}" install

## Maintaining Open CF-lite

If you want to maintain, enhance, extend, or otherwise modify Open
CF-lite, it is likely you will need to change its build system,
based on GNU autotools, in some circumstances.

After any change to the Open CF-lite build system, including any
*Makefile.am* files or the *configure.ac* file, you must run the
`autoreconf` to update the build system.

### Dependencies

Due to its leverage of GNU autotools, if you want to modify or
otherwise maintain the Open CF-lite build system, the following
additional packages are required and are invoked by `autoreconf`:

  * autoconf
  * automake
  * libtool

#### Linux

When supported on Linux, on Debian-based Linux distributions such as
Ubuntu, these Open CF-lite build system dependencies can be satisfied
with the following:

    % sudo apt-get install autoconf automake libtool

#### Mac OS X

On Mac OS X, these dependencies can be installed and satisfied using
[Brew](https://brew.sh/):

    % brew install autoconf automake libtool

# Interact

There are numerous avenues for Open CF-lite support:

  * Bugs and feature requests - [submit to the Issue Tracker](https://github.com/gerickson/opencflite/issues)

# Versioning

Open CF-lite follows Apple's upstream CoreFoundation versioning.

# License

Open CF-lite is released under the [Apple Public Source License 2.0 license](https://opensource.org/licenses/APSL-2.0).
See the `LICENSE` file for more information.

Enjoy!

* Brent Fulgham <bfulgham@gmail.com>
* Grant Erickson <gerickson@nuovations.com>
