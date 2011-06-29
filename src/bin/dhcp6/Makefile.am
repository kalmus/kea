SUBDIRS = . tests

AM_CPPFLAGS = -I$(top_srcdir)/src/lib -I$(top_builddir)/src/lib
AM_CPPFLAGS += -I$(top_srcdir)/src/bin -I$(top_builddir)/src/bin
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/dns -I$(top_builddir)/src/lib/dns
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/cc -I$(top_builddir)/src/lib/cc
AM_CPPFLAGS += -I$(top_srcdir)/src/lib/asiolink
AM_CPPFLAGS += -I$(top_builddir)/src/lib/asiolink
AM_CPPFLAGS += $(BOOST_INCLUDES)

AM_CXXFLAGS = $(B10_CXXFLAGS)

if USE_STATIC_LINK
AM_LDFLAGS = -static
endif

pkglibexecdir = $(libexecdir)/@PACKAGE@

CLEANFILES = *.gcno *.gcda spec_config.h

man_MANS = b10-dhcp6.8
EXTRA_DIST = $(man_MANS) dhcp6.spec

#if ENABLE_MAN
#b10-dhcp6.8: b10-dhcp6.xml
#	xsltproc --novalid --xinclude --nonet -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $(srcdir)/b10-dhcp6.xml
#endif

spec_config.h: spec_config.h.pre
	$(SED) -e "s|@@LOCALSTATEDIR@@|$(localstatedir)|" spec_config.h.pre >$@

BUILT_SOURCES = spec_config.h
pkglibexec_PROGRAMS = b10-dhcp6
b10_dhcp6_SOURCES = main.cc
b10_dhcp6_SOURCES += dhcp6.h
b10_dhcp6_LDADD =  $(top_builddir)/src/lib/datasrc/libdatasrc.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/dns/libdns++.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/config/libcfgclient.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/cc/libcc.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/exceptions/libexceptions.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/asiodns/libasiodns.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/asiolink/libasiolink.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/log/liblog.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/xfr/libxfr.la
b10_dhcp6_LDADD += $(top_builddir)/src/lib/server_common/libserver_common.la
b10_dhcp6_LDADD += $(SQLITE_LIBS)

# TODO: config.h.in is wrong because doesn't honor pkgdatadir
# and can't use @datadir@ because doesn't expand default ${prefix}
b10_dhcp6dir = $(pkgdatadir)
b10_dhcp6_DATA = dhcp6.spec
