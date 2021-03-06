SUBDIRS = . tests

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(KEA_CXXFLAGS)
if USE_CLANGPP
# Disable unused parameter warning caused by some Boost headers when compiling with clang
AM_CXXFLAGS += -Wno-unused-parameter
endif

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

CLEANFILES  = *.gcno *.gcda spec_config.h dhcp4_messages.h dhcp4_messages.cc s-messages

man_MANS = kea-dhcp4.8
DISTCLEANFILES = $(man_MANS)
EXTRA_DIST = $(man_MANS) kea-dhcp4.xml dhcp4.spec

if GENERATE_DOCS
kea-dhcp4.8: kea-dhcp4.xml
	@XSLTPROC@ --novalid --xinclude --nonet -o $@ \
        http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl \
	$(srcdir)/kea-dhcp4.xml

else

$(man_MANS):
	@echo Man generation disabled.  Creating dummy $@.  Configure with --enable-generate-docs to enable it.
	@echo Man generation disabled.  Remove this file, configure with --enable-generate-docs, and rebuild Kea > $@

endif

spec_config.h: spec_config.h.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" spec_config.h.pre >$@

dhcp4_messages.h dhcp4_messages.cc: s-messages

s-messages: dhcp4_messages.mes
	$(top_builddir)/src/lib/log/compiler/message $(top_srcdir)/src/bin/dhcp4/dhcp4_messages.mes
	touch $@

BUILT_SOURCES = spec_config.h dhcp4_messages.h dhcp4_messages.cc

# convenience archive

noinst_LTLIBRARIES = libdhcp4.la

libdhcp4_la_SOURCES  =
libdhcp4_la_SOURCES += ctrl_dhcp4_srv.cc ctrl_dhcp4_srv.h
libdhcp4_la_SOURCES += json_config_parser.cc json_config_parser.h
libdhcp4_la_SOURCES += dhcp4_log.cc dhcp4_log.h
libdhcp4_la_SOURCES += dhcp4_srv.cc dhcp4_srv.h

if CONFIG_BACKEND_BUNDY
libdhcp4_la_SOURCES += bundy_controller.cc
endif

if CONFIG_BACKEND_JSON
libdhcp4_la_SOURCES += kea_controller.cc
endif

nodist_libdhcp4_la_SOURCES = dhcp4_messages.h dhcp4_messages.cc
EXTRA_DIST += dhcp4_messages.mes

sbin_PROGRAMS = kea-dhcp4

kea_dhcp4_SOURCES  = main.cc

kea_dhcp4_LDADD  = libdhcp4.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/dhcp/libkea-dhcp++.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/dhcp_ddns/libkea-dhcp_ddns.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/util/libkea-util.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/dhcpsrv/libkea-dhcpsrv.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/exceptions/libkea-exceptions.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/asiolink/libkea-asiolink.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/log/libkea-log.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/config/libkea-cfgclient.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/cc/libkea-cc.la
kea_dhcp4_LDADD += $(top_builddir)/src/lib/hooks/libkea-hooks.la

kea_dhcp4dir = $(pkgdatadir)
kea_dhcp4_DATA = dhcp4.spec
