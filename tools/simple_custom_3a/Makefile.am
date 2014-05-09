AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = \
	libcustomized_3a.la

#includes
ALLINCLUDES = \
    -I$(top_srcdir)

# src
IUTILSRC = Customized3A.cpp

libcustomized_3a_la_SOURCES = $(IUTILSRC)

#cpphacks
CPPHACKS = -std=c++11 \
   -D__user= \
   -DPAGESIZE=4096 \
   -Dstrlcpy=strncpy \
   -DHAVE_LINUX_OS \
   -DBYPASS_MODE \
   -DHAVE_PTHREADS \
   -DCAMERA_HAL_DEBUG

libcustomized_3a_la_CPPFLAGS = $(CPPHACKS)


libcustomized_3a_la_CPPFLAGS += $(ALLINCLUDES)

.PHONY: rpm
rpm:
	./rpm/build_rpm.sh $(DESTDIR)
