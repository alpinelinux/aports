.POSIX:

objects = affine.o automata.o bits.o cells.o commands.o constants.o coxgroup.o coxtypes.o error.o fcoxgroup.o files.o general.o graph.o help.o interactive.o interface.o invkl.o io.o kl.o klsupport.o main.o memory.o minroots.o posets.o sage.o schubert.o special.o transducer.o type.o typeA.o uneqkl.o wgraph.o

coxeter: libcoxeter3.so.0.0.0
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. -o coxeter main.o -l:libcoxeter3.so.0.0.0

libcoxeter3.so.0.0.0: $(objects)
	$(CXX) $(CXXFLAGS) -shared -Wl,-soname=libcoxeter3.so.0.0.0 -o libcoxeter3.so.0.0.0 $(objects)

.cpp.o:
	$(CXX) $(CXXFLAGS) -fPIC -c -o $@ $<
