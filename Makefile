include /opt/fpp/src/makefiles/common/setup.mk

all: libfpp-zcpp.so
debug: all

OBJECTS_fpp_ZCPP_so += src/FPPZCPP.o
LIBS_fpp_ZCPP_so += -L/opt/fpp/src -lfpp
CXXFLAGS_src/FPPZCPP.o += -I/opt/fpp/src

%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-zcpp.so: $(OBJECTS_fpp_ZCPP_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_ZCPP_so) $(LIBS_fpp_ZCPP_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-zcpp.so $(OBJECTS_fpp_ZCPP_so)
