
INSTPATH=/usr/local/
FLAGS=-Os -fPIC

.PHONY: all

all: libretranse.a libretranse.so retranse

retranse_parse.o: retranse_parse.cpp retranse_node.hpp dirlist.hpp
	$(CXX) $(FLAGS) -c retranse_parse.cpp

retranse_run.o: retranse_run.cpp retranse_parse.hpp retranse_node.hpp retranse.hpp
	$(CXX) $(FLAGS) -c retranse_run.cpp

.cpp.o: 
	$(CXX) $(FLAGS) -c $<

libretranse.a: retranse_run.o retranse_parse.o 
	ar r libretranse.a retranse_run.o retranse_parse.o

libretranse.so: retranse_run.o retranse_parse.o
	$(CXX) $(CFLAGS) -shared -o libretranse.so retranse_run.o retranse_parse.o -lpcre++ -lpcre

retranse: retranse.o libretranse.a
	$(CXX) $(FLAGS) -L. -o retranse retranse.o -lretranse -lpcre++ -lpcre


.PHONY: clean

clean:
	rm -f *.o

cleanall: clean
	rm -f libretranse.* retranse

.PHONY: install

install: all
	cp libretranse.a $(INSTPATH)lib
	cp libretranse.so $(INSTPATH)lib
	cp retranse.hpp $(INSTPATH)include
	cp retranse $(INSTPATH)bin

.PHONY: uninstall

uninstall:
	rm -f $(INSTPATH)lib/libretranse.a
	rm -f $(INSTPATH)lib/libretranse.so
	rm -f $(INSTPATH)include/retranse.hpp
	rm -f $(INSTPATH)bin/retranse


