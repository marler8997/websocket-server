HTTP=../http-parser
MOREC=../morec/more
CC=gcc -I ${MOREC}

websocketserver: ${MOREC}/sha1.o ${MOREC}/base64.o ${HTTP}/http_parser.o net.o websocket.c
	${CC} -o $@ -I . -I ${HTTP} $^

net.o: net.c net.h
	${CC} -o $@ -c $<

#weblib/sha1.o: weblib/sha1.c
#	${CC} -o $@ -c $<

#weblib/base64.o: weblib/base64.c
#	${CC} -o $@ -c $<

#sha1.o: ${MOREC}/sha1.c
#	${CC} -o $@ -c $^
#base64.o: ${MOREC}/base64.c
#	${CC} -o $@ -c $^

clean:
	rm -rf *~ websocketserver *.o weblib/*.o

