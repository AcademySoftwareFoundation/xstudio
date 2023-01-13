TEMPLATE = subdirs

CONFIG += ordered

SUBDIRS += buildlib tests/quickfutureunittests

quickfutureunittests.depends = buildlib
