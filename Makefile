HTTP=../http-parser
MOREC=../morec
CC=gcc -I ${MOREC}

websocketserver: ${MOREC}/more/sha1.o ${MOREC}/more/base64.o ${HTTP}/http_parser.o net.o websocketFrameParser.c websocket.c
	${CC} -o $@ -I . -I ${HTTP} -I ${MOREC} $^

websocketFrameParser.o: websocketFrameParser.c
	${CC} -o $@ -I . -I ${MOREC} $^

net.o: net.c net.h
	${CC} -o $@ -c $<

#weblib/sha1.o: weblib/sha1.c
#	${CC} -o $@ -c $<

#weblib/base64.o: weblib/base64.c
#	${CC} -o $@ -c $<

#sha1.o: ${MOREC}/more/sha1.c
#	${CC} -o $@ -c $^
#base64.o: ${MOREC}/more/base64.c
#	${CC} -o $@ -c $^

clean:
	rm -rf *~ websocketserver *.o weblib/*.o

