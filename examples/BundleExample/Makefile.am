AM_CFLAGS		= -I${top_srcdir}/include

EXTRA_DIST		= BundleExample.vcproj

if CF_BUILD_TESTS
check_PROGRAMS		= BundleExample
endif

BundleExample_LDADD	= ${top_builddir}/libCoreFoundation.la

BundleExample_SOURCES	= BundleExample.c

if CF_BUILD_TESTS

# One of the use cases that the example runs through is validating the
# bundle for this package itself. However, since the package may or
# may not be installed at the time of the example execution and may or
# may not be installed in the default path, we install a temporary
# instance of the bundle using existing install rules, run the
# example, and then remove the temporary instance.

CFBUNDLE_EXAMPLE_TMPDIR := $(shell mktemp -u -d /tmp/tmp.CFBundleExample-XXXXXXXX)
CFBUNDLE_EXAMPLE_DIR    := $(CFBUNDLE_EXAMPLE_TMPDIR)${datadir}/CoreServices
CFBUNDLE_EXAMPLE_DICT   := $(CFBUNDLE_EXAMPLE_DIR)/Info.plist

#
# execute-example-with-bundle [ <tool>  [ <tool arguments> ] ]
#
# Execute the 'BundleExample' program, optionally through the
# specified tool with optional tool arguments, with the specified
# example bundle directory. On completion, unconditionally clean up
# the example bundle directory.
#
#   tool           - An optionally-specified tool such as 'gdb' or
#                    'valgrind' through which the example is to be
#                    run.
#   tool arguments - Optionally-specified arguments passed to 'tool'.
#
define execute-example-with-bundle
	${LIBTOOL} --mode execute $(1) $(2) ./BundleExample "$(CFBUNDLE_EXAMPLE_DIR)"
	-$(MAKE) -C ${top_builddir} DESTDIR=$(CFBUNDLE_EXAMPLE_TMPDIR) uninstall
	-rm -rf $(CFBUNDLE_EXAMPLE_TMPDIR)
endef # execute-example-with-bundle

$(CFBUNDLE_EXAMPLE_TMPDIR):
	mkdir -p "$(@)"

$(CFBUNDLE_EXAMPLE_DICT): | $(CFBUNDLE_EXAMPLE_TMPDIR)
	$(MAKE) -C ${top_builddir} DESTDIR=$(CFBUNDLE_EXAMPLE_TMPDIR) install-data

check gdb valgrind: $(CFBUNDLE_EXAMPLE_DICT)
check gdb valgrind: BundleExample

check:
	$(call execute-example-with-bundle,,)

gdb:
	$(call execute-example-with-bundle,${@},)

valgrind:
	$(call execute-example-with-bundle,${@},${VALGRINDFLAGS})

endif #CF_BUILD_TESTS
